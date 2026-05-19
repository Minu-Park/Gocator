#include <string>
#include <vector>
#include <iostream>

extern int run_cli(int argc, char** argv);

#ifdef GOCATOR_HAS_UI
extern int run_ui(int argc, char** argv);
#else
int run_ui(int argc, char** argv)
{
    std::cerr << "UI was not built. Please run with CLI arguments.\n";
    return 1;
}
#endif

int main(int argc, char** argv)
{
    // If no arguments or if the first argument isn't a known CLI command, run UI.
    // Known CLI commands from cli_main.cpp: discover, info, grab, profile-test, etc.
    
    bool use_ui = (argc == 1);
    
    if (argc >= 2)
    {
        std::string first_arg = argv[1];
        if (first_arg == "--ui" || first_arg == "-gui")
        {
            use_ui = true;
        }
        else if (first_arg == "--help" || first_arg == "-h" || first_arg == "discover" || first_arg == "help" || first_arg == "debug")
        {
            use_ui = false;
        }
        else
        {
            // Check if 'debug' is the second argument (e.g., <ip> debug)
            if (argc >= 3 && std::string(argv[2]) == "debug")
            {
                use_ui = false;
            }
            // If it looks like an IP, it's likely CLI (e.g., <sensor-ip> info)
            else if (first_arg.find('.') != std::string::npos)
            {
                use_ui = false;
            }
            else
            {
                // Default to UI if unknown
                use_ui = true;
            }
        }
    }

    if (use_ui)
    {
        return run_ui(argc, argv);
    }
    else
    {
        return run_cli(argc, argv);
    }
}
