set cmake_call=cmake -H. -Bbuild -G "Visual Studio 16 2019" -A x64 -DGWENI_RENDER_SDL2=OFF -DGWENI_RENDER_OPENGL=OFF -DGWENI_RENDER_OPENGL_CORE=ON

echo %cmake_call%
call %cmake_call%