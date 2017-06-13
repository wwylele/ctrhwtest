#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "vshader_shbin.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct { float position[3]; float texcoord[2]; float normal[3]; } vertex;

static const vertex vertex_list[] =
{
	// First face (PZ)
	// First triangle
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {+0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	// Second triangle
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {-0.5f, +0.5f, +0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },

};

#define vertex_list_count (sizeof(vertex_list)/sizeof(vertex_list[0]))

static PrintConsole bottomScreen;

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection, uLoc_modelView, uLoc_normal, uLoc_view;
static C3D_Mtx projection;

static C3D_LightEnv lightEnv;
static C3D_Light light;
static C3D_LightLut lut;

static void* vbo_data;

C3D_FVec lightVec = { { 1.0, -0.5, 0.0, 0.0 } };

static float lut_func(float angle, float a)
{
    return 1.0;
}

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection   = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView    = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	uLoc_normal       = shaderInstanceGetUniformLocation(program.vertexShader, "normal");
	uLoc_view         = shaderInstanceGetUniformLocation(program.vertexShader, "view");

	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // v2=normal

	// Create the VBO (vertex buffer object)
	vbo_data = linearAlloc(sizeof(vertex_list));
	memcpy(vbo_data, vertex_list, sizeof(vertex_list));

	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, vbo_data, sizeof(vertex), 3, 0x210);

	// Configure the first fragment shading substage to blend the fragment primary color
	// with the fragment secondary color.
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_FRAGMENT_SECONDARY_COLOR, 0, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	static const C3D_Material material =
	{
		{ 0.0f, 0.0f, 0.0f }, //ambient
		{ 0.0f, 0.0f, 0.0f }, //diffuse
		{ 1.0f, 1.0f, 1.0f }, //specular0
		{ 0.0f, 0.0f, 0.0f }, //specular1
		{ 0.0f, 0.0f, 0.0f }, //emission
	};

	C3D_LightEnvInit(&lightEnv);
	C3D_LightEnvBind(&lightEnv);
	C3D_LightEnvMaterial(&lightEnv, &material);
	C3D_LightEnvClampHighlights(&lightEnv, false);

	LightLut_FromFunc(&lut, lut_func, 0, true);
	C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_NH, true, &lut);

	C3D_LightInit(&light, &lightEnv);
	C3D_LightColor(&light, 1.0, 1.0, 1.0);
	C3D_LightPosition(&light, &lightVec);
	C3D_LightTwoSideDiffuse(&light, true);
}

float normal[3] = {0.0, 0.0, 1.0};
float view[3] = {0.0, 1.0, 0.0};
float lightv[3] = {1.0, 0.0, 0.0};

static void sceneRender(float iod)
{
	// Compute the projection matrix
	Mtx_PerspStereoTilt(&projection, 40.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 1000.0f, iod, 2.0f, false);
	
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_normal, normal[0], normal[1], normal[2], 0.0);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_view, view[0], view[1], view[2], 0.0);
	
	lightVec.x = lightv[0];
	lightVec.y = lightv[1];
	lightVec.z = lightv[2];
	lightVec.w = 0.0;
	
	C3D_LightPosition(&light, &lightVec);
	    
	float pos[] = {-0.6, +0.6};
	
	for (int i = 0; i < 2; ++i) {
	    C3D_LightGeoFactor(&light, 0, i == 0);
	    
	    C3D_Mtx modelView;
	    Mtx_Identity(&modelView);
	    Mtx_Translate(&modelView, pos[i], 0.0, -3.0, true);
	
	    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView,  &modelView);

	    // Draw the VBO
	    C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_list_count);
	}
}

static void sceneExit(void)
{
	// Free the VBO
	linearFree(vbo_data);

	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

float dot(float a[], float b[]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

FILE* record;
void updateText() {
    consoleClear();
    printf("\x1b[0;0HHello!\n");
    printf("[A]Normal= % 2.5f, % 2.5f, % 2.5f\n", normal[0], normal[1], normal[2]);
    printf("[B]View  = % 2.5f, % 2.5f, % 2.5f\n", view[0], view[1], view[2]);
    printf("[X]Light = % 2.5f, % 2.5f, % 2.5f\n", lightv[0], lightv[1], lightv[2]);
    
    //fprintf(record, "\n");
    //fprintf(record, "% 3.5f, % 3.5f, % 3.5f,", normal[0], normal[1], normal[2]);
    //fprintf(record, "% 3.5f, % 3.5f, % 3.5f,", view[0], view[1], view[2]);
    //fprintf(record, "% 3.5f, % 3.5f, % 3.5f,", lightv[0], lightv[1], lightv[2]);
    
    u8* fb = (u8*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    u8* left = fb + 3 * (120 + 100 * 240);
    u8* right = fb + 3 * (120 + 300 * 240);
    printf("left=%02X, right = %02X\ng(measured)   = %0.5f\n", left[0], right[0], (float)left[0]/right[0]);
    //fprintf(record, "%0.5f", (float)left[0]/right[0]);
    

    float d = 1 + dot(lightv, view);
    float g = d == 0.0 ? 0.0 : 0.5 * fabs(dot(lightv, normal)) / (1 + dot(lightv, view));
    if (g < 0) g = 0;
    if (g > 1) g = 1;
    
    printf("g(calculated) = %0.5f", g);
    
}

void moveVector(float v[], float dx, float dy) {
    v[0] += dx;
    v[1] += dy;
    float r = v[0]*v[0] + v[1]*v[1];
    if (r >= 1.0) {
        r = sqrt(r);
        v[0] /= r;
        v[1] /= r;
        v[2] = 0;
    } else {
        v[2] = sqrt(1-r);
    }

}

int main()
{
	// Initialize graphics
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);
	gfxSetDoubleBuffering(GFX_BOTTOM, true);
	
	gfxSet3D(true); // Enable stereoscopic 3D
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);


	// Initialize the render targets
	C3D_RenderTarget* targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(targetLeft,   C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetClear(targetRight,  C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the scene
	sceneInit();
	
	// record = fopen("geo_factor.txt", "wt");

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();
		
		circlePosition circle;
		hidCircleRead(&circle);
		
		float dx = circle.dx / 5000.0;
		float dy = circle.dy / 5000.0;

		// Respond to user input
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
			
		if (kHeld & KEY_A) {
		    moveVector(normal, dx, dy);
		}
		
		if (kHeld & KEY_B) {
		    moveVector(view, dx, dy);
		}
		
		if (kHeld & KEY_X) {
		    moveVector(lightv, dx, dy);
		}

		float slider = osGet3DSliderState();
		float iod = slider/3;
		
		

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		{
			C3D_FrameDrawOn(targetLeft);
			sceneRender(-iod);

			if (iod > 0.0f)
			{
				C3D_FrameDrawOn(targetRight);
				sceneRender(iod);
			}
		}
		C3D_FrameEnd(0);
		
		static int k = 0;
	
		if (k == 4)updateText();
		/*if (k == 0) {
		    static int normal_phi = 0;
		    static int normal_theta = 0;
		    static int light_theta = 0;
		    
		    int factor = 4;
		    
		    normal[0] = cos(normal_theta / (5.0 * factor) * M_PI) * sin(normal_phi / (20.0 * factor) * M_PI);
		    normal[1] = sin(normal_theta / (5.0 * factor) * M_PI) * sin(normal_phi / (20.0 * factor) * M_PI);
		    normal[2] = cos(normal_phi / (20.0 * factor) * M_PI);
		    
		    lightv[0] = view[0] = cos(light_theta / (10.0 * factor) * M_PI);
		    lightv[1] = view[1] = sin(light_theta / (10.0 * factor) * M_PI);
		    lightv[2] = view[2] = 0;
		    
		    lightv[1] = -lightv[1];
		    
		    normal_theta ++;
		    if (normal_theta == 10 * factor) {
		        normal_theta = 0;
		        normal_phi ++;
		        if (normal_phi == 10 * factor + 1) {
		            normal_phi = 0;
		            light_theta ++;
		            if (light_theta == 10 * factor) {
		                break;
		            }
		        }
		    }
		}*/
		
		++k;
		k %= 8;
	}
	
	// fclose(record);
	

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
