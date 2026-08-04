#include "cli.hpp"

int main(int argc, char** argv)
{
    core::initialize();

    CmdLine cmd_line(argc, argv);
    cmd_line.setup();
    cmd_line.run();

    if(DEBUG_ON) $PRINT_UNIMPLEMENTED_FEATURES();
    return EXIT_SUCCESS;
}
