#include "openfile.h"

int openBiosFile( char * r )
{
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    int result;
    NSArray *filesToOpen;

    [oPanel setAllowsMultipleSelection:NO];
    [oPanel setTitle:@"Open Bios File..."];
    result = [oPanel runModalForDirectory:nil file:nil types:nil];
    if (result == NSOKButton)
    {
        filesToOpen = [oPanel filenames];
        NSString *aFile = [filesToOpen objectAtIndex:0];
        strcpy(r , [aFile UTF8String]);
        return 1;
    }
    return 0;
}

int openFile( char * r )

{
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    int result;
    NSArray *filesToOpen;

    [oPanel setAllowsMultipleSelection:NO];
    [oPanel setTitle:@"Open Rom..."];
    result = [oPanel runModalForDirectory:nil file:nil types:nil];
    if (result == NSOKButton)
    {
        filesToOpen = [oPanel filenames];
        NSString *aFile = [filesToOpen objectAtIndex:0];
        strcpy(r , [aFile UTF8String]);
        return 1;
    }
    return 0;
}
