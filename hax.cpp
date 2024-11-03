#include <windows.h>
#include <psapi.h>

#include <fmt/xchar.h>

// Create a bunch of pointers for functions and data accessed by the game,
// these values have been retrieved by the disassembler

void **ptr_data_4b9e44 = (void **)0x4b9e44;
int32_t *ptr_data_62627c = (int32_t*)0x62627c;

void(__fastcall *sub_440940)(int32_t*) = (void(__fastcall *)(int32_t*))0x440940;
void(__fastcall *sub_4409f0)(int32_t*) = (void(__fastcall *)(int32_t*))0x4409f0;
void(__fastcall *sub_441330)(int32_t*) = (void(__fastcall *)(int32_t*))0x441330;
void(__fastcall *sub_43ee50)(int32_t*) = (void(__fastcall *)(int32_t*))0x43ee50;
void(__fastcall *sub_43d2f0)(int32_t*) = (void(__fastcall *)(int32_t*))0x43d2f0;
void(__fastcall *sub_43d880)(int32_t*) = (void(__fastcall *)(int32_t*))0x43d880;
void(__fastcall *sub_441e80)(int32_t*) = (void(__fastcall *)(int32_t*))0x441e80;

void(__thiscall *sub_450d60)(void*, int32_t*) = (void(__thiscall *)(void*, int32_t*))0x450d60;

// These two functions have been named as I have figured out a few things about them
int32_t(*subtract_life_or_enable_bomb_timer)(int32_t*) = (int32_t(*)(int32_t*))0x440cf0;
void(*this_function_important_for_item_pickup_and_player_death)(int32_t*) = (void(*)(int32_t*))0x4411c0;

// Start address of the orginal function
void *check_player_collision_ptr = (void*)0x441fb0;

bool player_is_invincible = true;

// Reimplementation (with the help of the disassembler) of the original function
int32_t __fastcall check_player_collision(int32_t* arg1)
{
    int32_t* var_8 = arg1;
    
    if (*ptr_data_62627c == 0)
    {
        sub_440940(arg1);
        sub_4409f0(arg1);

        // OK so, 0x902 appears to be a state variable
        // indicating whether the player is alive, dying or dead
        // 3 appears to be alive, 2 appears to be dying, 1 appears to be dead
        // 0 may be the post-death invincibility state?
        if (player_is_invincible)
            arg1[0x902] = 3;
        
        // Appears to do some sort of collision check, disabling these
        // branches appears to freeze the player
        if (((int32_t)arg1[0x902]) == 2)
        {
            auto res = subtract_life_or_enable_bomb_timer(arg1);

            // Modifying this branch gives us invincibility
            if (res != 0)
                this_function_important_for_item_pickup_and_player_death(arg1);
        }
        else if (((int32_t)arg1[0x902]) == 1)
            this_function_important_for_item_pickup_and_player_death(arg1);
        
        sub_441330(arg1);
        
        // Making this always branch will fix the freezing issue but
        // also means that the player cannot pick up score items
        if ((((int32_t)arg1[0x902]) != 2 && ((int32_t)arg1[0x902]) != 1))
            sub_43ee50(arg1);
        
        //data_4b9e44;
        sub_450d60(*ptr_data_4b9e44, arg1);
        
        if (((int32_t)*(uint8_t*)((char*)arg1 + 0x240a)) != 0)
        {
            //data_4b9e44;
            sub_450d60(*ptr_data_4b9e44, &arg1[0x93]);
            //data_4b9e44;
            sub_450d60(*ptr_data_4b9e44, &arg1[0x126]);
        }
        
        sub_43d2f0(arg1);
        sub_43d880(arg1);
        sub_441e80(arg1);
    }
    
    return 1;
}

std::string win32_error_as_string(DWORD error) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
    std::string message{messageBuffer, size};

    LocalFree(messageBuffer);

    return message;
}

void print_error(const std::string &func) {
    auto code = GetLastError();

    auto msg = fmt::format("{} failed ({}): {}\n", func, code, win32_error_as_string(code));

    MessageBoxA(NULL, msg.c_str(), "DLL Message", MB_OK);
}

bool inject(HANDLE process) {
    // Redirect the original function to point to our custom function
    uint8_t redirect_check_player_collision[] = {
        // x86 relative jmp
        0xE9, 0x00, 0x00, 0x00, 0x00
    };

    // Relative offset from the function we are patching and the address of our function
    *(int32_t*)(redirect_check_player_collision + 1) = (int32_t)check_player_collision - (int32_t)check_player_collision_ptr - 5;

    // Mark the memory as rwx so we can patch the function to jump to ours
    DWORD old_protect;
    if(VirtualProtect(check_player_collision_ptr, sizeof(redirect_check_player_collision), PAGE_EXECUTE_READWRITE, &old_protect) == 0) {
        print_error("VirtualProtect");
        return false;
    }
    
    memcpy(check_player_collision_ptr, redirect_check_player_collision, sizeof(redirect_check_player_collision));
    
    VirtualProtect(check_player_collision_ptr, sizeof(redirect_check_player_collision), old_protect, &old_protect);
    return true;
}

bool WINAPI DllMain(
    HINSTANCE hinstance,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpvReserved)  // reserved
{
    if (fdwReason != DLL_PROCESS_ATTACH) {
        return true;
    }

    auto proc = GetCurrentProcess();

    return inject(proc);
}
