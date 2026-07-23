@echo off
setlocal

echo === Building instrumented executable ===
gcc -O3 -DNDEBUG -flto=auto -march=native -mtune=native -ffast-math -fprofile-generate -x c -std=c99 -c ./include/flecs.c -I./include -o ./build/flecs.o && g++ -std=c++26 -O3 -DNDEBUG -flto=auto -march=native -mtune=native -ffast-math -fomit-frame-pointer -fprofile-generate -mwindows main.cpp ./build/flecs.o -I./include ./include/libraylib.a -lopengl32 -lgdi32 -lwinmm -lws2_32 -ldbghelp -o game_pgo.exe || exit /b

echo.
echo === Run the game and generate profile data ===
start /wait "" game_pgo.exe

echo.
echo === Building optimized executable ===
gcc -O3 -DNDEBUG -flto=auto -march=native -mtune=native -ffast-math -fprofile-use -fprofile-correction -x c -std=c99 -c ./include/flecs.c -I./include -o ./build/flecs.o && g++ -std=c++26 -O3 -DNDEBUG -flto=auto -march=native -mtune=native -ffast-math -fomit-frame-pointer -fprofile-use -fprofile-correction -mwindows main.cpp ./build/flecs.o -I./include ./include/libraylib.a -lopengl32 -lgdi32 -lwinmm -lws2_32 -ldbghelp -o "C:\Users\Student\Desktop\game.exe" && copy /Y "C:\Users\Student\Desktop\game.exe" game.exe

echo.
echo Done.
pause
