#include <windows.h>
#include <stdlib.h>

extern int main_real( int, char** );

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow )
{
 	return main_real( __argc, __argv );
}

int main(int argc, char *argv[])
{
	return main_real( argc, argv );
}
