::glslangvalidator -V %~dp0update_counter_begin.comp -o %~dp0update_counter_begin.comp.spv
::glslangvalidator -V %~dp0particle_emit.comp -o %~dp0particle_emit.comp.spv
glslangvalidator -V %~dp0particle_simulate.comp -o %~dp0particle_simulate.comp.spv
::glslangvalidator -V %~dp0update_counter_end.comp -o %~dp0update_counter_end.comp.spv
::glslangvalidator -V %~dp0particle.frag -o %~dp0particle.frag.spv
::glslangvalidator -V %~dp0particle.vert -o %~dp0particle.vert.spv
::glslangvalidator -V %~dp0scene.frag -o %~dp0scene.frag.spv
::glslangvalidator -V %~dp0scene.vert -o %~dp0scene.vert.spv
::pause


