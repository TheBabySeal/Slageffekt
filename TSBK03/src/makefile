all : shadow

shadow: shadowmap.cpp ../common/GL_utilities.c ../common/VectorUtils3.c ../common/LoadTGA.c ../common/loadobj.c ../common/Linux/MicroGlut.c
	gcc -Wall -o shadow -DGL_GLEXT_PROTOTYPES shadowmap.cpp ../common/GL_utilities.c ../common/VectorUtils3.c ../common/LoadTGA.c ../common/loadobj.c ../common/Linux/MicroGlut.c -I../common -I../common/Linux -lm -lGL -lX11 -lstdc++

clean :
	rm shadow
