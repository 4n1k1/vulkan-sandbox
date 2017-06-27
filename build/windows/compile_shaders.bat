%VULKAN_SDK%\Bin32\glslangValidator.exe -V ..\..\src\shaders\shader.vert -o shaders\vertex.spv
%VULKAN_SDK%\Bin32\glslangValidator.exe -V ..\..\src\shaders\shader.frag -o shaders\fragment.spv
%VULKAN_SDK%\Bin32\glslangValidator.exe -V ..\..\src\shaders\shader.comp -o shaders\compute.spv
pause
