// http://www.fabiensanglard.net/shadowmapping/index.php
// Win32 support removed. See original if you need it, but you should rather use GLEW or glee.

// Unlike what is stated at the page above, this runs just fine on the Mac, in my case both compiled and from Lightweight IDE.
// So uploading a pre-compiled binary is no problem.

// Reorganized a bit
// Removed most ARB and EXT
// Replaced the clumsy startTranslate/endTranslate with APPLY_ON_BOTH
// (from old non-shader shadow mapping demo).
// Put in more interesting objects.

// 2012: Ported to OpenGL 3.2. APPLY_ON_BOTH replaced by explicit transformations.

#ifdef __APPLE__
	#include <OpenGL/gl3.h>
	// uses framework Cocoa
#else
	#include <GL/gl.h>
#endif

#include "MicroGlut.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "LoadTGA.h"
#include "VectorUtils3.h"
#include "GL_utilities.h"
#include "loadobj.h"

#include <deque>
#include <map>
#include <iostream>
#include <mutex>

// ------- Application code --------

// Expressed as float so gluPerspective division returns a float and not 0 (640/480 != 640.0/480.0).
#define RENDER_WIDTH 800.0
#define RENDER_HEIGHT 600.0

// We assign one texture unit in which to store the transformation.
#define TEX_UNIT_OPAQUE 0
#define TEX_UNIT_TRANSPARENT 1

// Camera angles
#define SHOW_FLAT_SWORD 1
#define SHOW_MODEL_SWORD 2
#define SHOW_BOTH_SWORDS 3

// variables for checking if swords should be drawn
int show_flat_sword = 0;
int show_model_sword = 1;

// Sword positions
Point3D p_sword_model_trail = {-25, 6, 0};
Point3D p_sword_flat_trail = {-75, 6, 0};

//Camera position
Point3D p_camera_angle_1 = p_sword_flat_trail + Point3D(0, 40, 50);
Point3D p_camera_angle_2 = p_sword_model_trail + Point3D(0, 40, 50);
Point3D p_camera_angle_3 = {-50, 70, 70};
Point3D p_camera = p_camera_angle_2;

//Camera lookAt
Point3D l_camera_angle_3 = {-50, 0, 0};
Point3D l_camera = p_sword_model_trail;

//Light mouvement circle radius -- currently not used
float light_mvnt = 40.0f; // At 30 we get edge artifacts

//Light position
Point3D p_light = {-55,40,0};

//Light lookAt
Point3D l_light = {-50, 0, 0};

// Shader IDs and locations for texUnits
GLuint OpaqueShaderId, plainShaderId, TransparentShaderId;
GLuint OpaqueShaderTexUnitLoc, TransShadowsTexUnitLoc;

FBOstruct *fbo_opaque, *fbo_transparent;

float old_t = 0;
GLfloat alpha = 0.20;
GLuint trail_length = 100;
GLfloat spin_speed = 0;

GLfloat trailColor[] = {1.0, 0.0, 0.0};

std::mutex mutex;

#define NEAR 1.0
#define FAR 300.0
#define W 600
#define H 600

//-----------------------------matrices------------------------------
mat4	modelViewMatrix, textureMatrix;
mat4 projectionMatrix;

// deques containing matrices for the trail
std::deque<Point3D> trail_points;
std::deque<float> time_points;

std::deque<mat4> trail_mv_matrices;
std::deque<mat4> trail_tx_matrices;

std::deque<mat4> trail_shadow_mv_matrices;
std::deque<mat4> trail_shadow_tx_matrices;

// Arrays for ground and the Model references
GLfloat groundcolor[] = {0.3f,0.3f,0.3f,1};
GLfloat ground[] = {
							-100,2,-35,
							-100,2, 35,
							0,2, 35,
							0,2,-35
							};
GLuint groundIndices[] = {0, 1, 2, 0, 2, 3};

Model *groundModel, *swordModel, *trailModel;

// Calculate distance between two Point3D
float distance(Point3D point1, Point3D point2)
{
	return sqrt(pow(point2.x - point1.x, 2) + pow(point2.y - point1.y, 2) + pow(point2.z - point1.z, 2));
}

// Rotate a 3D vector with angle theta
Point3D rotateAroundYAxis(Point3D vector, float theta)
{
	return Point3D(vector.x*cos(theta) + vector.z*sin(theta),
								 vector.y,
								 -vector.x*sin(theta) + vector.z*cos(theta));
}

void setCameraAngle(int angle)
{
	if(angle == SHOW_FLAT_SWORD)
	{
		l_camera = p_sword_flat_trail;
		p_camera = p_camera_angle_1;
		show_flat_sword = 1;
		show_model_sword = 0;
	}
	else if(angle == SHOW_MODEL_SWORD)
	{
		l_camera = p_sword_model_trail;
		p_camera = p_camera_angle_2;
		show_model_sword = 1;
		show_flat_sword = 0;
	}
	else if(angle == SHOW_BOTH_SWORDS)
	{
		l_camera = l_camera_angle_3;
		p_camera = p_camera_angle_3;

		if (show_flat_sword && show_model_sword)
		{
			show_flat_sword = 0;
			show_model_sword = 0;
		}
		else
		{
			show_flat_sword = 1;
			show_model_sword = 1;
		}
	}
}

void loadShadowShader()
{
	OpaqueShaderId = loadShaders("../extra/OpaqueShader.vert", "../extra/OpaqueShader.frag");
	OpaqueShaderTexUnitLoc = glGetUniformLocation(OpaqueShaderId,"textureUnit");
	plainShaderId = loadShaders("../extra/plain.vert", "../extra/plain.frag");
	TransparentShaderId = loadShaders("../extra/TransparentShader.vert", "../extra/TransparentShader.frag");
	TransShadowsTexUnitLoc = glGetUniformLocation(OpaqueShaderId, "transShadowsTexUnit");
}

// This update only change the position of the light.
void updatePositions(void)
{
	p_light.x = light_mvnt * cos(glutGet(GLUT_ELAPSED_TIME)/1000.0);
	p_light.z = light_mvnt * sin(glutGet(GLUT_ELAPSED_TIME)/1000.0);
}

// Build the transformation sequence for the light source path,
// by copying from the ordinary camera matrices.
void setTextureMatrix(void)
{
	mat4 scaleBiasMatrix;

	textureMatrix = IdentityMatrix();

	// Scale and bias transform, moving from unit cube [-1,1] to [0,1]
	scaleBiasMatrix = Mult(T(0.5, 0.5, 0.0), S(0.5, 0.5, 1.0));
	textureMatrix = Mult(Mult(scaleBiasMatrix, projectionMatrix), modelViewMatrix);
	// Multiply modelview and transformation matrices
}

void loadObjects(void)
{
// The shader must be loaded before this is called!
	if (OpaqueShaderId == 0)
		printf("Warning! Is the shader not loaded?\n");

	groundModel = LoadDataToModel(
			ground,
			NULL,
			NULL,
			groundcolor,
			groundIndices,
			4,
			6);

	swordModel = LoadModelPlus("../objects/sword.obj");
	CenterModel(swordModel);
	ReloadModelData(swordModel);
}

void updateTrail()
{
	while(trail_points.size() >= trail_length)
	{
		trail_points.pop_back();
		time_points.pop_back();
		trail_mv_matrices.pop_back();
		trail_tx_matrices.pop_back();
		trail_shadow_mv_matrices.pop_back();
		trail_shadow_tx_matrices.pop_back();
	}
}

void drawOpaqueObjects(GLuint shader, GLuint create_shadow_map)
{
	glUniformMatrix4fv(glGetUniformLocation(shader, "projectionMatrix"), 1, GL_TRUE, projectionMatrix.m);

	// Ground
	glUniform4f(glGetUniformLocation(shader, "shade"), 0.45, 0.45, 0.45, 0.45);
	glUniform1f(glGetUniformLocation(shader, "alpha"), alpha);
	glUniformMatrix4fv(glGetUniformLocation(shader, "modelViewMatrix"), 1, GL_TRUE, modelViewMatrix.m);
	glUniformMatrix4fv(glGetUniformLocation(shader, "textureMatrix"), 1, GL_TRUE, textureMatrix.m);
	DrawModel(groundModel, shader, "in_Position", NULL, NULL);

	// Sword

	float t = glutGet(GLUT_ELAPSED_TIME)/1000.0;
	float pi = 3.14;
	mat4 rot = Rz(pi/2)*Ry(pi/2);
	mat4 trans = T(p_sword_model_trail.x, p_sword_model_trail.y, p_sword_model_trail.z);
	mat4 total = trans*rot*Rz(spin_speed*t)*T(0,15,0)*S(5.0, 5.0, 5.0);
	mat4 mv = modelViewMatrix*total;
	mat4 tx = textureMatrix*total;

	if(create_shadow_map)
	{
		trail_shadow_mv_matrices.push_front(mv);
		trail_shadow_tx_matrices.push_front(tx);
		trail_points.push_front(p_sword_model_trail);
		time_points.push_front(t);
	}
	else
	{
		trail_mv_matrices.push_front(mv);
		trail_tx_matrices.push_front(tx);
	}

	glUniform4f(glGetUniformLocation(shader, "shade"), 0.6, 0.6, 0.6, 0.6); // Lighter object
	glUniformMatrix4fv(glGetUniformLocation(shader, "modelViewMatrix"), 1, GL_TRUE, mv.m);
	glUniformMatrix4fv(glGetUniformLocation(shader, "textureMatrix"), 1, GL_TRUE, tx.m);

	// Draw sword with modelbased trail
	if(show_model_sword)
	{
		DrawModel(swordModel, shader, "in_Position", NULL, NULL);
	}

	trans = T(p_sword_flat_trail.x, p_sword_flat_trail.y, p_sword_flat_trail.z);
	total = trans*rot*Rz(spin_speed*t)*T(0,15,0)*S(5.0, 5.0, 5.0);
	mv = modelViewMatrix*total;
	tx = textureMatrix*total;

	glUniformMatrix4fv(glGetUniformLocation(shader, "modelViewMatrix"), 1, GL_TRUE, mv.m);
	glUniformMatrix4fv(glGetUniformLocation(shader, "textureMatrix"), 1, GL_TRUE, tx.m);

	// Draw sword with flat trail
	if(show_flat_sword)
	{
		DrawModel(swordModel, shader, "in_Position", NULL, NULL);
	}

}

void drawTrail(GLuint shader, GLuint create_shadow_map)
{

	if (spin_speed == 0)
	{
		return;
	}

	glUniformMatrix4fv(glGetUniformLocation(shader, "projectionMatrix"), 1, GL_TRUE, projectionMatrix.m);

	// Create the flat trail

	if(time_points.size() > 1)
	{
		GLfloat trail[6*time_points.size()];
		GLuint trailIndices[6*(time_points.size()-1)];
		for (uint i = 0; i < time_points.size(); ++i)
		{
			float t = time_points.at(i);
			Point3D p1 = rotateAroundYAxis(Point3D(-5, 0, 0), spin_speed*t) + p_sword_flat_trail;
			Point3D p2 = rotateAroundYAxis(Point3D(-26, 0, 0), spin_speed*t) + p_sword_flat_trail;

			trail[6*i] = p1.x;
			trail[6*i + 1] = p1.y;
			trail[6*i + 2] = p1.z;
			trail[6*i + 3] = p2.x;
			trail[6*i + 4] = p2.y;
			trail[6*i + 5] = p2.z;

			if(i < time_points.size() - 1)
			{
				if(spin_speed > 0)
				{
					trailIndices[i*6 + 0] = 2*i+0;
					trailIndices[i*6 + 1] = 2*i+3;
					trailIndices[i*6 + 2] = 2*i+1;
					trailIndices[i*6 + 3] = 2*i+0;
					trailIndices[i*6 + 4] = 2*i+2;
					trailIndices[i*6 + 5] = 2*i+3;
				}
				else
				{ // (0,1,3),(0,3,2)
					trailIndices[i*6 + 0] = 2*i+0;
					trailIndices[i*6 + 1] = 2*i+1;
					trailIndices[i*6 + 2] = 2*i+3;
					trailIndices[i*6 + 3] = 2*i+0;
					trailIndices[i*6 + 4] = 2*i+3;
					trailIndices[i*6 + 5] = 2*i+2;
				}
			}
		}

		trailModel = LoadDataToModel(
			trail,
			NULL,
			NULL,
			NULL,
			trailIndices,
			2*time_points.size(),
			6*(time_points.size()-1)
		);

		// Draw the flat trail
		glUniform1f(glGetUniformLocation(shader, "alpha"), alpha);
		glUniform3f(glGetUniformLocation(shader, "shade"), trailColor[0], trailColor[1], trailColor[2]);
		glUniformMatrix4fv(glGetUniformLocation(shader, "modelViewMatrix"), 1, GL_TRUE, modelViewMatrix.m);
		glUniformMatrix4fv(glGetUniformLocation(shader, "textureMatrix"), 1, GL_TRUE, textureMatrix.m);

		if(show_flat_sword)
		{
			DrawModel(trailModel, shader, "in_Position", NULL, NULL);
		}
	}

	// Draw trail based on sword models

	glUniform3f(glGetUniformLocation(shader, "shade"), trailColor[0], trailColor[1], trailColor[2]);

	std::map<float, int> map;

	// Sort the trail based on distance from camera
	int id = 1;
	std::deque<Point3D>::iterator it;
	for (it = trail_points.begin()+1; it != trail_points.end(); ++it)
	{
		Point3D it_pos = *it;

		float dist = distance(it_pos, p_camera);

		if (map.count(dist) > 0)
		{
			dist -= 0.01*map.size();
		}

		map[dist] = ++id;
	}

	// Draw the trail farthest -> closest to camera
	mat4 mv, tx;
	for(std::map<float,int>::reverse_iterator iter = map.rbegin(); iter != map.rend(); ++iter)
	{
		int id = iter->second;

		if (create_shadow_map)
		{
			mv = trail_shadow_mv_matrices[id];
			tx = trail_shadow_tx_matrices[id];
		}
		else
		{
			mv = trail_mv_matrices[id];
			tx = trail_tx_matrices[id];
		}

		glUniform1f(glGetUniformLocation(shader, "alpha"), alpha);
		glUniformMatrix4fv(glGetUniformLocation(shader, "modelViewMatrix"), 1, GL_TRUE, mv.m);
		glUniformMatrix4fv(glGetUniformLocation(shader, "textureMatrix"), 1, GL_TRUE, tx.m);

		if(show_model_sword)
		{
			DrawModel(swordModel, shader, "in_Position", NULL, NULL);
		}
	}

}

void renderScene(void)
{
	mutex.lock();
	// Change light positions
	//updatePositions();
	updateTrail();

	//glCullFace(GL_FRONT);

	// Setup projection matrix
	projectionMatrix = perspective(140, RENDER_WIDTH/RENDER_HEIGHT, 10, 400);

	// Setup the modelview from the light source
	modelViewMatrix = lookAt(p_light.x, p_light.y, p_light.z,
				l_light.x, l_light.y, l_light.z, 0,1,0);
	// Using the result from lookAt, add a bias to position the result properly in texture space
	setTextureMatrix();
	// 1. Render scene to FBO

	useFBO(fbo_opaque, NULL, NULL);
	glViewport(0,0,RENDER_WIDTH,RENDER_HEIGHT);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE); // Depth only
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(OpaqueShaderId);
	glUniform1i(OpaqueShaderTexUnitLoc,TEX_UNIT_OPAQUE);
	glActiveTexture(GL_TEXTURE0 + TEX_UNIT_OPAQUE);
	glBindTexture(GL_TEXTURE_2D,0);

	//Using the simple shader
	drawOpaqueObjects(plainShaderId, 1);

	useFBO(fbo_transparent, NULL, NULL);
	glViewport(0,0,RENDER_WIDTH,RENDER_HEIGHT);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE); // Depth only
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUniform1i(TransShadowsTexUnitLoc,TEX_UNIT_TRANSPARENT);
	glActiveTexture(GL_TEXTURE0 + TEX_UNIT_TRANSPARENT);
	glBindTexture(GL_TEXTURE_2D,0);

	drawTrail(plainShaderId, 1);

	//glFlush(); //Causes problems on the Mac, but might be needed on other platforms
	//2. Render from camera.
	// Now rendering from the camera POV

	useFBO(NULL, fbo_opaque, fbo_transparent);

	glViewport(0,0,RENDER_WIDTH,RENDER_HEIGHT);

	//Enabling color write (previously disabled for light POV z-buffer rendering)
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// Clear previous frame values
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Setup the modelview from the camera
	modelViewMatrix = lookAt(p_camera.x, p_camera.y, p_camera.z,
					l_camera.x, l_camera.y, l_camera.z, 0,1,0);

	// Setup projection matrix
	projectionMatrix = perspective(60, RENDER_WIDTH/RENDER_HEIGHT, 1, 400);

	glCullFace(GL_BACK);

	// Draw ground from camera angle
	glUseProgram(OpaqueShaderId);
	glUniform1i(OpaqueShaderTexUnitLoc,TEX_UNIT_OPAQUE);
	glActiveTexture(GL_TEXTURE0 + TEX_UNIT_OPAQUE);
	glBindTexture(GL_TEXTURE_2D,fbo_opaque->depth);

	glUniform1i(TransShadowsTexUnitLoc,TEX_UNIT_TRANSPARENT);
	glActiveTexture(GL_TEXTURE0 + TEX_UNIT_TRANSPARENT);
	glBindTexture(GL_TEXTURE_2D,fbo_transparent->depth);

	drawOpaqueObjects(OpaqueShaderId, 0);

	// Draw transparent objects without shadow
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(TransparentShaderId);

	drawTrail(TransparentShaderId, 0);
	glDisable(GL_BLEND);

	glutSwapBuffers();

	mutex.unlock();
}

void processNormalKeys(unsigned char key, int x, int y)
{
	mutex.lock();

	if (key == 27)
		exit(0);

	if (key == '-')
		spin_speed -= 0.5;

	if (key == '+')
		spin_speed += 0.5;

	if (key == 'k')
		trail_length += 2;

	if (key == 'l')
	{
		trail_length -= 2;
		trail_length = std::max(1, (int)trail_length);
	}

	if (key == 'r')
	{
		trailColor[0] = !trailColor[0];
	}

	if (key == 'g')
	{
		trailColor[1] = !trailColor[1];
	}

	if (key == 'b')
	{
		trailColor[2] = !trailColor[2];
	}

	if (key == '1')
	{
		setCameraAngle(1);
	}
	if (key == '2')
	{
		setCameraAngle(2);
	}
	if (key == '3')
	{
		setCameraAngle(3);
	}

	if (key == 'a')
	{
		if(alpha >= 1)
		{
			alpha = 0.0;
		}
		else
		{
			alpha += 0.05;
		}
	}

	mutex.unlock();
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(RENDER_WIDTH,RENDER_HEIGHT);
	glutInitContextVersion(3, 2); // Might not be needed in Linux
	glutCreateWindow("Sword Animation Demo");

	loadShadowShader();
	loadObjects();
	fbo_opaque = initFBO2(RENDER_WIDTH,RENDER_HEIGHT, 0, 1);
	fbo_transparent = initFBO2(RENDER_WIDTH, RENDER_HEIGHT, 0, 1);

	glEnable(GL_DEPTH_TEST);
	glClearColor(0,0,0,1.0f);
	glEnable(GL_CULL_FACE);

	glutDisplayFunc(renderScene);
	glutRepeatingTimerFunc(5);
	glutKeyboardFunc(processNormalKeys);

	glutMainLoop();
	exit(0);
}
