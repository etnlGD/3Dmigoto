#include "Main.h"
#include <Shlobj.h>
#include <Winuser.h>
#include "../DirectInput.h"

FILE *LogFile = 0;
static bool gInitialized = false;
static int SCREEN_WIDTH = -1;
static int SCREEN_HEIGHT = -1;
static int SCREEN_REFRESH = -1;
static int SCREEN_FULLSCREEN = -1;
static bool gForceStereo = false;
static bool gCreateStereoProfile = false;
bool LogInput = false;
static bool take_screenshot = false;

ThreadSafePointerSet D3D11Wrapper::ID3D10Device::m_List;
ThreadSafePointerSet D3D11Wrapper::ID3D10Multithread::m_List;

void InitializeDLL()
{
	if (!gInitialized)
	{
		gInitialized = true;
		wchar_t dir[MAX_PATH];
		GetModuleFileName(0, dir, MAX_PATH);
		wcsrchr(dir, L'\\')[1] = 0;
		wcscat(dir, L"d3dx.ini");
		LogFile = GetPrivateProfileInt(L"Logging", L"calls", 0, dir) ? (FILE *)-1 : 0;
		if (LogFile) fopen_s(&LogFile, "d3d10_log.txt", "w");
		LogInput = GetPrivateProfileInt(L"Logging", L"input", 0, dir) == 1;
		wchar_t val[MAX_PATH];
		int read = GetPrivateProfileString(L"Device", L"width", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_WIDTH);
		read = GetPrivateProfileString(L"Device", L"height", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_HEIGHT);
		read = GetPrivateProfileString(L"Device", L"refresh_rate", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_REFRESH);
		read = GetPrivateProfileString(L"Device", L"full_screen", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_FULLSCREEN);
		gForceStereo = GetPrivateProfileInt(L"Device", L"force_stereo", 0, dir) == 1;
		gCreateStereoProfile = GetPrivateProfileInt(L"Stereo", L"create_profile", 0, dir) == 1;

		// DirectInput
		InputDevice[0] = 0;
		GetPrivateProfileString(L"Rendering", L"Input", 0, InputDevice, MAX_PATH, dir);
		wchar_t *end = InputDevice + wcslen(InputDevice) - 1; while (end > InputDevice && isspace(*end)) end--; *(end+1) = 0;
		InputDeviceId = GetPrivateProfileInt(L"Rendering", L"DeviceNr", -1, dir);
		// Actions
		GetPrivateProfileString(L"Rendering", L"next_pixelshader", 0, InputAction[0], MAX_PATH, dir);
		end = InputAction[0] + wcslen(InputAction[0]) - 1; while (end > InputAction[0] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_pixelshader", 0, InputAction[1], MAX_PATH, dir);
		end = InputAction[1] + wcslen(InputAction[1]) - 1; while (end > InputAction[1] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_pixelshader", 0, InputAction[2], MAX_PATH, dir);
		end = InputAction[2] + wcslen(InputAction[2]) - 1; while (end > InputAction[2] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"take_screenshot", 0, InputAction[3], MAX_PATH, dir);
		end = InputAction[3] + wcslen(InputAction[3]) - 1; while (end > InputAction[3] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"next_indexbuffer", 0, InputAction[4], MAX_PATH, dir);
		end = InputAction[4] + wcslen(InputAction[4]) - 1; while (end > InputAction[4] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_indexbuffer", 0, InputAction[5], MAX_PATH, dir);
		end = InputAction[5] + wcslen(InputAction[5]) - 1; while (end > InputAction[5] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_indexbuffer", 0, InputAction[6], MAX_PATH, dir);
		end = InputAction[6] + wcslen(InputAction[6]) - 1; while (end > InputAction[6] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"next_vertexshader", 0, InputAction[7], MAX_PATH, dir);
		end = InputAction[7] + wcslen(InputAction[7]) - 1; while (end > InputAction[7] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_vertexshader", 0, InputAction[8], MAX_PATH, dir);
		end = InputAction[8] + wcslen(InputAction[8]) - 1; while (end > InputAction[8] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_vertexshader", 0, InputAction[9], MAX_PATH, dir);
		end = InputAction[9] + wcslen(InputAction[9]) - 1; while (end > InputAction[9] && isspace(*end)) end--; *(end+1) = 0;
		InitDirectInput();
		// XInput
		XInputDeviceId = GetPrivateProfileInt(L"Rendering", L"XInputDevice", -1, dir);		

		if (LogFile) fprintf(LogFile, "DLL initialized.\n");
	}
}

void DestroyDLL()
{
	if (LogFile)
	{
		if (LogFile) fprintf(LogFile, "Destroying DLL...\n");
		fclose(LogFile);
	}
}

// D3DCompiler bridge
struct D3D11BridgeData
{
	UINT64 BinaryHash;
	char *HLSLFileName;
};

typedef ULONG 	D3DKMT_HANDLE;
typedef int		KMTQUERYADAPTERINFOTYPE;

typedef struct _D3DKMT_QUERYADAPTERINFO 
{
  D3DKMT_HANDLE           hAdapter;
  KMTQUERYADAPTERINFOTYPE Type;
  VOID                    *pPrivateDriverData;
  UINT                    PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;

typedef void *D3D10DDI_HRTADAPTER;
typedef void *D3D10DDI_HADAPTER;
typedef void D3DDDI_ADAPTERCALLBACKS;
typedef void D3D10DDI_ADAPTERFUNCS;
typedef void D3D10_2DDI_ADAPTERFUNCS;

typedef struct D3D10DDIARG_OPENADAPTER 
{
  D3D10DDI_HRTADAPTER           hRTAdapter;
  D3D10DDI_HADAPTER             hAdapter;
  UINT                          Interface;
  UINT                          Version;
  const D3DDDI_ADAPTERCALLBACKS *pAdapterCallbacks;
  union {
    D3D10DDI_ADAPTERFUNCS   *pAdapterFuncs;
    D3D10_2DDI_ADAPTERFUNCS *pAdapterFuncs_2;
  };
} D3D10DDIARG_OPENADAPTER;

static HMODULE hD3D10 = 0;
typedef HRESULT (WINAPI *tD3D10CompileEffectFromMemory)(void *pData, SIZE_T DataLength, LPCSTR pSrcFileName, 
	const D3D11Base::D3D10_SHADER_MACRO *pDefines, D3D11Base::ID3D10Include *pInclude, UINT HLSLFlags, UINT FXFlags, 
	D3D11Base::ID3D10Blob **ppCompiledEffect, D3D11Base::ID3D10Blob **ppErrors);
static tD3D10CompileEffectFromMemory _D3D10CompileEffectFromMemory;
typedef HRESULT (WINAPI *tD3D10CompileShader)(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, 
	const D3D11Base::D3D10_SHADER_MACRO *pDefines, D3D11Base::LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, 
	LPCSTR pProfile, UINT Flags, D3D11Base::ID3D10Blob **ppShader, D3D11Base::ID3D10Blob **ppErrorMsgs);
static tD3D10CompileShader _D3D10CompileShader;
typedef HRESULT (WINAPI *tD3D10CreateBlob)(SIZE_T NumBytes, D3D11Base::LPD3D10BLOB *ppBuffer);
static tD3D10CreateBlob _D3D10CreateBlob;
typedef HRESULT (WINAPI *tD3D10CreateDevice)(D3D11Base::IDXGIAdapter *pAdapter, D3D11Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D11Base::ID3D10Device **ppDevice);
static tD3D10CreateDevice _D3D10CreateDevice;
typedef HRESULT (WINAPI *tD3D10CreateDeviceAndSwapChain)(D3D11Base::IDXGIAdapter *pAdapter, D3D11Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D11Base::DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, 
	D3D11Base::IDXGISwapChain **ppSwapChain, D3D11Base::ID3D10Device **ppDevice);
static tD3D10CreateDeviceAndSwapChain _D3D10CreateDeviceAndSwapChain;
typedef HRESULT (WINAPI *tD3D10CreateEffectFromMemory)(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D11Base::ID3D10Device *pDevice, D3D11Base::ID3D10EffectPool *pEffectPool, D3D11Base::ID3D10Effect **ppEffect);
static tD3D10CreateEffectFromMemory _D3D10CreateEffectFromMemory;
typedef HRESULT (WINAPI *tD3D10CreateEffectPoolFromMemory)(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D11Base::ID3D10Device *pDevice, D3D11Base::ID3D10EffectPool **ppEffectPool);
static tD3D10CreateEffectPoolFromMemory _D3D10CreateEffectPoolFromMemory;
typedef HRESULT (WINAPI *tD3D10CreateStateBlock)(D3D11Base::ID3D10Device *pDevice, D3D11Base::D3D10_STATE_BLOCK_MASK *pStateBlockMask,
	D3D11Base::ID3D10StateBlock **ppStateBlock);
static tD3D10CreateStateBlock _D3D10CreateStateBlock;
typedef HRESULT (WINAPI *tD3D10DisassembleEffect)(D3D11Base::ID3D10Effect *pEffect, BOOL EnableColorCode, D3D11Base::ID3D10Blob **ppDisassembly);
static tD3D10DisassembleEffect _D3D10DisassembleEffect;
typedef HRESULT (WINAPI *tD3D10DisassembleShader)(const void *pShader, SIZE_T BytecodeLength, BOOL EnableColorCode, LPCSTR pComments, 
	D3D11Base::ID3D10Blob **ppDisassembly);
static tD3D10DisassembleShader _D3D10DisassembleShader;
typedef LPCSTR (WINAPI *tD3D10GetGeometryShaderProfile)(D3D11Base::ID3D10Device *pDevice);
static tD3D10GetGeometryShaderProfile _D3D10GetGeometryShaderProfile;
typedef HRESULT (WINAPI *tD3D10GetInputAndOutputSignatureBlob)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10Blob **ppSignatureBlob);
static tD3D10GetInputAndOutputSignatureBlob _D3D10GetInputAndOutputSignatureBlob;
typedef HRESULT (WINAPI *tD3D10GetInputSignatureBlob)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10Blob **ppSignatureBlob);
static tD3D10GetInputSignatureBlob _D3D10GetInputSignatureBlob;
typedef HRESULT (WINAPI *tD3D10GetOutputSignatureBlob)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10Blob **ppSignatureBlob);
static tD3D10GetOutputSignatureBlob _D3D10GetOutputSignatureBlob;
typedef LPCSTR (WINAPI *tD3D10GetPixelShaderProfile)(D3D11Base::ID3D10Device *pDevice);
static tD3D10GetPixelShaderProfile _D3D10GetPixelShaderProfile;
typedef HRESULT (WINAPI *tD3D10GetShaderDebugInfo)(const void *pShaderBytecode, SIZE_T BytecodeLength, D3D11Base::ID3D10Blob **ppDebugInfo);
static tD3D10GetShaderDebugInfo _D3D10GetShaderDebugInfo;
typedef int (WINAPI *tD3D10GetVersion)();
static tD3D10GetVersion _D3D10GetVersion;
typedef LPCSTR (WINAPI *tD3D10GetVertexShaderProfile)(D3D11Base::ID3D10Device *pDevice);
static tD3D10GetVertexShaderProfile _D3D10GetVertexShaderProfile;
typedef HRESULT (WINAPI *tD3D10PreprocessShader)(LPCSTR pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, 
	const D3D11Base::D3D10_SHADER_MACRO *pDefines, D3D11Base::LPD3D10INCLUDE pInclude, 
	D3D11Base::ID3D10Blob **ppShaderText, D3D11Base::ID3D10Blob **ppErrorMsgs);
static tD3D10PreprocessShader _D3D10PreprocessShader;
typedef HRESULT (WINAPI *tD3D10ReflectShader)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10ShaderReflection **ppReflector);
static tD3D10ReflectShader _D3D10ReflectShader;
typedef int (WINAPI *tD3D10RegisterLayers)();
static tD3D10RegisterLayers _D3D10RegisterLayers;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskDifference)(D3D11Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D11Base::D3D10_STATE_BLOCK_MASK *pB, D3D11Base::D3D10_STATE_BLOCK_MASK *pResult);
static tD3D10StateBlockMaskDifference _D3D10StateBlockMaskDifference;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskDisableAll)(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask);
static tD3D10StateBlockMaskDisableAll _D3D10StateBlockMaskDisableAll;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskDisableCapture)(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D11Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength);
static tD3D10StateBlockMaskDisableCapture _D3D10StateBlockMaskDisableCapture;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskEnableAll)(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask);
static tD3D10StateBlockMaskEnableAll _D3D10StateBlockMaskEnableAll;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskEnableCapture)(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D11Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength);
static tD3D10StateBlockMaskEnableCapture _D3D10StateBlockMaskEnableCapture;
typedef BOOL (WINAPI *tD3D10StateBlockMaskGetSetting)(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D11Base::D3D10_DEVICE_STATE_TYPES StateType, UINT Entry);
static tD3D10StateBlockMaskGetSetting _D3D10StateBlockMaskGetSetting;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskIntersect)(D3D11Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D11Base::D3D10_STATE_BLOCK_MASK *pB, D3D11Base::D3D10_STATE_BLOCK_MASK *pResult);
static tD3D10StateBlockMaskIntersect _D3D10StateBlockMaskIntersect;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskUnion)(D3D11Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D11Base::D3D10_STATE_BLOCK_MASK *pB, D3D11Base::D3D10_STATE_BLOCK_MASK *pResult);
static tD3D10StateBlockMaskUnion _D3D10StateBlockMaskUnion;
typedef int (WINAPI *tD3DKMTQueryAdapterInfo)(_D3DKMT_QUERYADAPTERINFO *);
static tD3DKMTQueryAdapterInfo _D3DKMTQueryAdapterInfo;
typedef int (WINAPI *tOpenAdapter10)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10 _OpenAdapter10;
typedef int (WINAPI *tOpenAdapter10_2)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10_2 _OpenAdapter10_2;
typedef int (WINAPI *tD3D11CoreCreateDevice)(__int32, int, int, LPCSTR lpModuleName, int, int, int, int, int, int);
static tD3D11CoreCreateDevice _D3D11CoreCreateDevice;
typedef int (WINAPI *tD3D11CoreCreateLayeredDevice)(int a, int b, int c, int d, int e);
static tD3D11CoreCreateLayeredDevice _D3D11CoreCreateLayeredDevice;
typedef int (WINAPI *tD3D11CoreGetLayeredDeviceSize)(int a, int b);
static tD3D11CoreGetLayeredDeviceSize _D3D11CoreGetLayeredDeviceSize;
typedef int (WINAPI *tD3D11CoreRegisterLayers)(int a, int b);
static tD3D11CoreRegisterLayers _D3D11CoreRegisterLayers;

static void InitD310()
{
	if (hD3D10) return;
	InitializeDLL();
	wchar_t sysDir[MAX_PATH];
	SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
	wcscat(sysDir, L"\\d3d10.dll");
	hD3D10 = LoadLibrary(sysDir);	
    if (hD3D10 == NULL)
    {
        if (LogFile) fprintf(LogFile, "LoadLibrary on d3d10.dll failed\n");
        
        return;
    }

	_D3D10CompileEffectFromMemory = (tD3D10CompileEffectFromMemory) GetProcAddress(hD3D10, "D3D10CompileEffectFromMemory");
	_D3D10CompileShader = (tD3D10CompileShader) GetProcAddress(hD3D10, "D3D10CompileShader");
	_D3D10CreateBlob = (tD3D10CreateBlob) GetProcAddress(hD3D10, "D3D10CreateBlob");
	_D3D10CreateDevice = (tD3D10CreateDevice) GetProcAddress(hD3D10, "D3D10CreateDevice");
	_D3D10CreateDeviceAndSwapChain = (tD3D10CreateDeviceAndSwapChain) GetProcAddress(hD3D10, "D3D10CreateDeviceAndSwapChain");
	_D3D10CreateEffectFromMemory = (tD3D10CreateEffectFromMemory) GetProcAddress(hD3D10, "D3D10CreateEffectFromMemory");
	_D3D10CreateEffectPoolFromMemory = (tD3D10CreateEffectPoolFromMemory) GetProcAddress(hD3D10, "D3D10CreateEffectPoolFromMemory");
	_D3D10CreateStateBlock = (tD3D10CreateStateBlock) GetProcAddress(hD3D10, "D3D10CreateStateBlock");
	_D3D10DisassembleEffect = (tD3D10DisassembleEffect) GetProcAddress(hD3D10, "D3D10DisassembleEffect");
	_D3D10DisassembleShader = (tD3D10DisassembleShader) GetProcAddress(hD3D10, "D3D10DisassembleShader");
	_D3D10GetGeometryShaderProfile = (tD3D10GetGeometryShaderProfile) GetProcAddress(hD3D10, "D3D10GetGeometryShaderProfile");
	_D3D10GetInputAndOutputSignatureBlob = (tD3D10GetInputAndOutputSignatureBlob) GetProcAddress(hD3D10, "D3D10GetInputAndOutputSignatureBlob");
	_D3D10GetInputSignatureBlob = (tD3D10GetInputSignatureBlob) GetProcAddress(hD3D10, "D3D10GetInputSignatureBlob");
	_D3D10GetOutputSignatureBlob = (tD3D10GetOutputSignatureBlob) GetProcAddress(hD3D10, "D3D10GetOutputSignatureBlob");
	_D3D10GetPixelShaderProfile = (tD3D10GetPixelShaderProfile) GetProcAddress(hD3D10, "D3D10GetPixelShaderProfile");
	_D3D10GetShaderDebugInfo = (tD3D10GetShaderDebugInfo) GetProcAddress(hD3D10, "D3D10GetShaderDebugInfo");
	_D3D10GetVersion = (tD3D10GetVersion) GetProcAddress(hD3D10, "D3D10GetVersion");
	_D3D10GetVertexShaderProfile = (tD3D10GetVertexShaderProfile) GetProcAddress(hD3D10, "D3D10GetVertexShaderProfile");
	_D3D10PreprocessShader = (tD3D10PreprocessShader) GetProcAddress(hD3D10, "D3D10PreprocessShader");
	_D3D10ReflectShader = (tD3D10ReflectShader) GetProcAddress(hD3D10, "D3D10ReflectShader");
	_D3D10RegisterLayers = (tD3D10RegisterLayers) GetProcAddress(hD3D10, "D3D10RegisterLayers");
	_D3D10StateBlockMaskDifference = (tD3D10StateBlockMaskDifference) GetProcAddress(hD3D10, "D3D10StateBlockMaskDifference");
	_D3D10StateBlockMaskDisableAll = (tD3D10StateBlockMaskDisableAll) GetProcAddress(hD3D10, "D3D10StateBlockMaskDisableAll");
	_D3D10StateBlockMaskDisableCapture = (tD3D10StateBlockMaskDisableCapture) GetProcAddress(hD3D10, "D3D10StateBlockMaskDisableCapture");
	_D3D10StateBlockMaskEnableAll = (tD3D10StateBlockMaskEnableAll) GetProcAddress(hD3D10, "D3D10StateBlockMaskEnableAll");
	_D3D10StateBlockMaskEnableCapture = (tD3D10StateBlockMaskEnableCapture) GetProcAddress(hD3D10, "D3D10StateBlockMaskEnableCapture");
	_D3D10StateBlockMaskGetSetting = (tD3D10StateBlockMaskGetSetting) GetProcAddress(hD3D10, "D3D10StateBlockMaskGetSetting");
	_D3D10StateBlockMaskIntersect = (tD3D10StateBlockMaskIntersect) GetProcAddress(hD3D10, "D3D10StateBlockMaskIntersect");
	_D3D10StateBlockMaskUnion = (tD3D10StateBlockMaskUnion) GetProcAddress(hD3D10, "D3D10StateBlockMaskUnion");
	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo) GetProcAddress(hD3D10, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10) GetProcAddress(hD3D10, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2) GetProcAddress(hD3D10, "OpenAdapter10_2");
	_D3D11CoreCreateDevice = (tD3D11CoreCreateDevice) GetProcAddress(hD3D10, "D3D11CoreCreateDevice");
	_D3D11CoreCreateLayeredDevice = (tD3D11CoreCreateLayeredDevice) GetProcAddress(hD3D10, "D3D11CoreCreateLayeredDevice");
	_D3D11CoreGetLayeredDeviceSize = (tD3D11CoreGetLayeredDeviceSize) GetProcAddress(hD3D10, "D3D11CoreGetLayeredDeviceSize");
	_D3D11CoreRegisterLayers = (tD3D11CoreRegisterLayers) GetProcAddress(hD3D10, "D3D11CoreRegisterLayers");
}

HRESULT WINAPI D3D10CompileEffectFromMemory(void *pData, SIZE_T DataLength, LPCSTR pSrcFileName, 
	const D3D11Base::D3D10_SHADER_MACRO *pDefines, D3D11Base::ID3D10Include *pInclude, UINT HLSLFlags, UINT FXFlags, 
	D3D11Base::ID3D10Blob **ppCompiledEffect, D3D11Base::ID3D10Blob **ppErrors)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CompileEffectFromMemory called.\n");
	
	return (*_D3D10CompileEffectFromMemory)(pData, DataLength, pSrcFileName, 
		pDefines, pInclude, HLSLFlags, FXFlags, 
		ppCompiledEffect, ppErrors);
}

HRESULT WINAPI D3D10CompileShader(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, 
	const D3D11Base::D3D10_SHADER_MACRO *pDefines, D3D11Base::LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, 
	LPCSTR pProfile, UINT Flags, D3D11Base::ID3D10Blob **ppShader, D3D11Base::ID3D10Blob **ppErrorMsgs)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CompileShader called.\n");
	
	return (*_D3D10CompileShader)(pSrcData, SrcDataLen, pFileName, 
		pDefines, pInclude, pFunctionName, 
		pProfile, Flags, ppShader, ppErrorMsgs);
}

HRESULT WINAPI D3D10CreateBlob(SIZE_T NumBytes, D3D11Base::LPD3D10BLOB *ppBuffer)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CreateBlob called.\n");
	
	return (*_D3D10CreateBlob)(NumBytes, ppBuffer);
}

static void EnableStereo()
{
	if (!gForceStereo) return;

	// Prepare NVAPI for use in this application
	D3D11Base::NvAPI_Status status;
	status = D3D11Base::NvAPI_Initialize();
	if (status != D3D11Base::NVAPI_OK)
	{
		D3D11Base::NvAPI_ShortString errorMessage;
		NvAPI_GetErrorMessage(status, errorMessage);
		if (LogFile) fprintf(LogFile, "  stereo init failed: %s\n", errorMessage);		
	}
	else
	{
		// Check the Stereo availability
		D3D11Base::NvU8 isStereoEnabled;
		status = D3D11Base::NvAPI_Stereo_IsEnabled(&isStereoEnabled);
		// Stereo status report an error
		if ( status != D3D11Base::NVAPI_OK)
		{
			// GeForce Stereoscopic 3D driver is not installed on the system
			if (LogFile) fprintf(LogFile, "  stereo init failed: no stereo driver detected.\n");		
		}
		// Stereo is available but not enabled, let's enable it
		else if(D3D11Base::NVAPI_OK == status && !isStereoEnabled)
		{
			if (LogFile) fprintf(LogFile, "  stereo available and disabled. Enabling stereo.\n");		
			status = D3D11Base::NvAPI_Stereo_Enable();
			if (status != D3D11Base::NVAPI_OK)
				if (LogFile) fprintf(LogFile, "    enabling stereo failed.\n");		
		}

		if (gCreateStereoProfile)
		{
			if (LogFile) fprintf(LogFile, "  enabling registry profile.\n");		
			
			D3D11Base::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3D11Base::NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
		}
	}
}

static D3D11Base::IDXGIAdapter *ReplaceAdapter(D3D11Base::IDXGIAdapter *wrapper)
{
	if (!wrapper)
		return wrapper;
	if (LogFile) fprintf(LogFile, "  checking for adapter wrapper, handle = %x\n", wrapper);
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 } };
	D3D11Base::IDXGIAdapter *realAdapter;
	if (wrapper->GetParent(marker, (void **) &realAdapter) == 0x13bc7e32)
	{
		if (LogFile) fprintf(LogFile, "    wrapper found. replacing with original handle = %x\n", realAdapter);
		
		return realAdapter;
	}
	return wrapper;
}

HRESULT WINAPI D3D10CreateDevice(D3D11Base::IDXGIAdapter *pAdapter, D3D11Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D11Wrapper::ID3D10Device **ppDevice)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CreateDevice called with adapter = %x\n", pAdapter);
	
	D3D11Base::ID3D10Device *origDevice = 0;
	EnableStereo();
	HRESULT ret = (*_D3D10CreateDevice)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, SDKVersion, &origDevice);
	if (ret != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with HRESULT=%x\n", ret);
		
		return ret;
	}

	D3D11Wrapper::ID3D10Device *wrapper = D3D11Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
	if (!wrapper)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");
		
		origDevice->Release();
		return E_OUTOFMEMORY;
	}
	*ppDevice = wrapper;

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %x, wrapper = %x\n", ret, origDevice, wrapper);
	
	return ret;
}

HRESULT WINAPI D3D10CreateDeviceAndSwapChain(D3D11Base::IDXGIAdapter *pAdapter, D3D11Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D11Base::DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, 
	D3D11Base::IDXGISwapChain **ppSwapChain, D3D11Wrapper::ID3D10Device **ppDevice)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CreateDeviceAndSwapChain called with adapter = %x\n", pAdapter);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, " Windowed = %d\n", pSwapChainDesc->Windowed);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, " Width = %d\n", pSwapChainDesc->BufferDesc.Width);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, " Height = %d\n", pSwapChainDesc->BufferDesc.Height);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, " Refresh rate = %f\n", 
		(float) pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float) pSwapChainDesc->BufferDesc.RefreshRate.Denominator);

	if (SCREEN_FULLSCREEN >= 0 && pSwapChainDesc) pSwapChainDesc->Windowed = !SCREEN_FULLSCREEN;
	if (SCREEN_REFRESH >= 0 && pSwapChainDesc && !pSwapChainDesc->Windowed)
	{
		pSwapChainDesc->BufferDesc.RefreshRate.Numerator = SCREEN_REFRESH;
		pSwapChainDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (SCREEN_WIDTH >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Width = SCREEN_WIDTH;
	if (SCREEN_HEIGHT >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Height = SCREEN_HEIGHT;
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  calling with parameters width = %d, height = %d, refresh rate = %f, windowed = %d\n", 
		pSwapChainDesc->BufferDesc.Width, pSwapChainDesc->BufferDesc.Height, 
		(float) pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float) pSwapChainDesc->BufferDesc.RefreshRate.Denominator,
		pSwapChainDesc->Windowed);

	EnableStereo();

	D3D11Base::ID3D10Device *origDevice = 0;
	HRESULT ret = (*_D3D10CreateDeviceAndSwapChain)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, SDKVersion, 
		pSwapChainDesc, ppSwapChain, &origDevice);
	
	// Changed to recognize that >0 DXGISTATUS values are possible, not just S_OK.
	if (FAILED(ret))
	{
		if (LogFile) fprintf(LogFile, "  failed with HRESULT=%x\n", ret);
		return ret;
	}

	D3D11Wrapper::ID3D10Device *wrapper = D3D11Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");
		
		origDevice->Release();
		return E_OUTOFMEMORY;
	}
	*ppDevice = wrapper;

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %x, wrapper = %x\n", ret, origDevice, wrapper);
	
	return ret;
}

HRESULT WINAPI D3D10CreateEffectFromMemory(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D11Base::ID3D10Device *pDevice, D3D11Base::ID3D10EffectPool *pEffectPool, D3D11Base::ID3D10Effect **ppEffect)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CreateEffectFromMemory called.\n");
	
	return (*_D3D10CreateEffectFromMemory)(pData, DataLength, FXFlags, 
		pDevice, pEffectPool, ppEffect);
}

HRESULT WINAPI D3D10CreateEffectPoolFromMemory(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D11Base::ID3D10Device *pDevice, D3D11Base::ID3D10EffectPool **ppEffectPool)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CreateEffectPoolFromMemory called.\n");
	
	return (*_D3D10CreateEffectPoolFromMemory)(pData, DataLength, FXFlags, 
		pDevice, ppEffectPool);
}

HRESULT WINAPI D3D10CreateStateBlock(D3D11Base::ID3D10Device *pDevice, D3D11Base::D3D10_STATE_BLOCK_MASK *pStateBlockMask,
	D3D11Base::ID3D10StateBlock **ppStateBlock)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10CreateStateBlock called.\n");
	
	return (*_D3D10CreateStateBlock)(pDevice, pStateBlockMask,
		ppStateBlock);
}

HRESULT WINAPI D3D10DisassembleEffect(D3D11Base::ID3D10Effect *pEffect, BOOL EnableColorCode, D3D11Base::ID3D10Blob **ppDisassembly)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10DisassembleEffect called.\n");
	
	return (*_D3D10DisassembleEffect)(pEffect, EnableColorCode, ppDisassembly);
}

HRESULT WINAPI D3D10DisassembleShader(const void *pShader, SIZE_T BytecodeLength, BOOL EnableColorCode, LPCSTR pComments, 
	D3D11Base::ID3D10Blob **ppDisassembly)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10DisassembleShader called.\n");
	
	return (*_D3D10DisassembleShader)(pShader, BytecodeLength, EnableColorCode, pComments, 
		ppDisassembly);
}

LPCSTR WINAPI D3D10GetGeometryShaderProfile(D3D11Base::ID3D10Device *pDevice)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetGeometryShaderProfile called.\n");
	
	return (*_D3D10GetGeometryShaderProfile)(pDevice);
}

HRESULT WINAPI D3D10GetInputAndOutputSignatureBlob(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10Blob **ppSignatureBlob)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetInputAndOutputSignatureBlob called.\n");
	
	return (*_D3D10GetInputAndOutputSignatureBlob)(pShaderBytecode, BytecodeLength, 
		ppSignatureBlob);
}

HRESULT WINAPI D3D10GetInputSignatureBlob(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10Blob **ppSignatureBlob)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetInputSignatureBlob called.\n");
	
	return (*_D3D10GetInputSignatureBlob)(pShaderBytecode, BytecodeLength, 
		ppSignatureBlob);
}

HRESULT WINAPI D3D10GetOutputSignatureBlob(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10Blob **ppSignatureBlob)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetOutputSignatureBlob called.\n");
	
	return (*_D3D10GetOutputSignatureBlob)(pShaderBytecode, BytecodeLength, 
		ppSignatureBlob);
}

LPCSTR WINAPI D3D10GetPixelShaderProfile(D3D11Base::ID3D10Device *pDevice)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetPixelShaderProfile called.\n");
	
	return (*_D3D10GetPixelShaderProfile)(pDevice);
}

HRESULT WINAPI D3D10GetShaderDebugInfo(const void *pShaderBytecode, SIZE_T BytecodeLength, D3D11Base::ID3D10Blob **ppDebugInfo)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetShaderDebugInfo called.\n");
	
	return (*_D3D10GetShaderDebugInfo)(pShaderBytecode, BytecodeLength, ppDebugInfo);
}

int WINAPI D3D10GetVersion()
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetVersion called.\n");
	
	return (*_D3D10GetVersion)();
}

LPCSTR WINAPI D3D10GetVertexShaderProfile(D3D11Base::ID3D10Device *pDevice)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10GetVertexShaderProfile called.\n");
	
	return (*_D3D10GetVertexShaderProfile)(pDevice);
}

HRESULT WINAPI D3D10PreprocessShader(LPCSTR pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, 
	const D3D11Base::D3D10_SHADER_MACRO *pDefines, D3D11Base::LPD3D10INCLUDE pInclude, 
	D3D11Base::ID3D10Blob **ppShaderText, D3D11Base::ID3D10Blob **ppErrorMsgs)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10PreprocessShader called.\n");
	
	return (*_D3D10PreprocessShader)(pSrcData, SrcDataSize, pFileName, 
		pDefines, pInclude, 
		ppShaderText, ppErrorMsgs);
}

HRESULT WINAPI D3D10ReflectShader(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D11Base::ID3D10ShaderReflection **ppReflector)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10ReflectShader called.\n");
	
	return (*_D3D10ReflectShader)(pShaderBytecode, BytecodeLength, 
		ppReflector);
}

int WINAPI D3D10RegisterLayers()
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10RegisterLayers called.\n");
	
	return (*_D3D10RegisterLayers)();
}

HRESULT WINAPI D3D10StateBlockMaskDifference(D3D11Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D11Base::D3D10_STATE_BLOCK_MASK *pB, D3D11Base::D3D10_STATE_BLOCK_MASK *pResult)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskDifference called.\n");
	
	return (*_D3D10StateBlockMaskDifference)(pA, 
		pB, pResult);
}

HRESULT WINAPI D3D10StateBlockMaskDisableAll(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskDisableAll called.\n");
	
	return (*_D3D10StateBlockMaskDisableAll)(pMask);
}

HRESULT WINAPI D3D10StateBlockMaskDisableCapture(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D11Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskDisableCapture called.\n");
	
	return (*_D3D10StateBlockMaskDisableCapture)(pMask, 
		StateType, RangeStart, RangeLength);
}

HRESULT WINAPI D3D10StateBlockMaskEnableAll(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskEnableAll called.\n");
	
	return (*_D3D10StateBlockMaskEnableAll)(pMask);
}

HRESULT WINAPI D3D10StateBlockMaskEnableCapture(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D11Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskEnableCapture called.\n");
	
	return (*_D3D10StateBlockMaskEnableCapture)(pMask, 
		StateType, RangeStart, RangeLength);
}

BOOL WINAPI D3D10StateBlockMaskGetSetting(D3D11Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D11Base::D3D10_DEVICE_STATE_TYPES StateType, UINT Entry)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskGetSetting called.\n");
	
	return (*_D3D10StateBlockMaskGetSetting)(pMask, 
		StateType, Entry);
}

HRESULT WINAPI D3D10StateBlockMaskIntersect(D3D11Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D11Base::D3D10_STATE_BLOCK_MASK *pB, D3D11Base::D3D10_STATE_BLOCK_MASK *pResult)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskIntersect called.\n");
	
	return (*_D3D10StateBlockMaskIntersect)(pA, 
		pB, pResult);
}

HRESULT WINAPI D3D10StateBlockMaskUnion(D3D11Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D11Base::D3D10_STATE_BLOCK_MASK *pB, D3D11Base::D3D10_STATE_BLOCK_MASK *pResult)
{
	InitD310();
	if (LogFile) fprintf(LogFile, "D3D10StateBlockMaskUnion called.\n");
	
	return (*_D3D10StateBlockMaskUnion)(pA, 
		pB, pResult);
}

STDMETHODIMP D3D11Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
{
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	if (riid.Data1 == marker.Data1 && riid.Data2 == marker.Data2 && riid.Data3 == marker.Data3 && 
		riid.Data4[0] == marker.Data4[0] && riid.Data4[1] == marker.Data4[1] && riid.Data4[2] == marker.Data4[2] && riid.Data4[3] == marker.Data4[3] && 
		riid.Data4[4] == marker.Data4[4] && riid.Data4[5] == marker.Data4[5] && riid.Data4[6] == marker.Data4[6] && riid.Data4[7] == marker.Data4[7])
	{
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: requesting real ID3D10Device handle from %x\n", *ppvObj);
		
	    D3D11Wrapper::ID3D10Device *p = (D3D11Wrapper::ID3D10Device*) D3D11Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			if (LogFile) fprintf(LogFile, "  given pointer was already the real device.\n");
		}
		else
		{
			*ppvObj = ((D3D11Wrapper::ID3D10Device *)*ppvObj)->m_pDevice;
		}
		if (LogFile) fprintf(LogFile, "  returning handle = %x\n", *ppvObj);
		
		return 0x13bc7e31;
	}

	if (LogFile) fprintf(LogFile, "QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %x\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);
	bool d3d9device = riid.Data1 == 0xd0223b96 && riid.Data2 == 0xbf7a && riid.Data3 == 0x43fd && riid.Data4[0] == 0x92 && 
		riid.Data4[1] == 0xbd && riid.Data4[2] == 0xa4 && riid.Data4[3] == 0x3b && riid.Data4[4] == 0x0d && 
		riid.Data4[5] == 0x82 && riid.Data4[6] == 0xb9 && riid.Data4[7] == 0xeb;
	bool d3d10device = riid.Data1 == 0x9b7e4c0f && riid.Data2 == 0x342c && riid.Data3 == 0x4106 && riid.Data4[0] == 0xa1 && 
		riid.Data4[1] == 0x9f && riid.Data4[2] == 0x4f && riid.Data4[3] == 0x27 && riid.Data4[4] == 0x04 && 
		riid.Data4[5] == 0xf6 && riid.Data4[6] == 0x89 && riid.Data4[7] == 0xf0;
	bool d3d10multithread = riid.Data1 == 0x9b7e4e00 && riid.Data2 == 0x342c && riid.Data3 == 0x4106 && riid.Data4[0] == 0xa1 && 
		riid.Data4[1] == 0x9f && riid.Data4[2] == 0x4f && riid.Data4[3] == 0x27 && riid.Data4[4] == 0x04 && 
		riid.Data4[5] == 0xf6 && riid.Data4[6] == 0x89 && riid.Data4[7] == 0xf0;
	bool dxgidevice = riid.Data1 == 0x54ec77fa && riid.Data2 == 0x1377 && riid.Data3 == 0x44e6 && riid.Data4[0] == 0x8c && 
			 riid.Data4[1] == 0x32 && riid.Data4[2] == 0x88 && riid.Data4[3] == 0xfd && riid.Data4[4] == 0x5f && 
			 riid.Data4[5] == 0x44 && riid.Data4[6] == 0xc8 && riid.Data4[7] == 0x4c;
    bool dxgidevice1 = riid.Data1 == 0x77db970f && riid.Data2 == 0x6276 && riid.Data3 == 0x48ba && riid.Data4[0] == 0xba && 
			 riid.Data4[1] == 0x28 && riid.Data4[2] == 0x07 && riid.Data4[3] == 0x01 && riid.Data4[4] == 0x43 && 
			 riid.Data4[5] == 0xb4 && riid.Data4[6] == 0x39 && riid.Data4[7] == 0x2c;
    bool dxgidevice2 = riid.Data1 == 0x05008617 && riid.Data2 == 0xfbfd && riid.Data3 == 0x4051 && riid.Data4[0] == 0xa7 && 
			 riid.Data4[1] == 0x90 && riid.Data4[2] == 0x14 && riid.Data4[3] == 0x48 && riid.Data4[4] == 0x84 && 
			 riid.Data4[5] == 0xb4 && riid.Data4[6] == 0xf6 && riid.Data4[7] == 0xa9;
    bool unknown1 = riid.Data1 == 0x7abb6563 && riid.Data2 == 0x02bc && riid.Data3 == 0x47c4 && riid.Data4[0] == 0x8e && 
			 riid.Data4[1] == 0xf9 && riid.Data4[2] == 0xac && riid.Data4[3] == 0xc4 && riid.Data4[4] == 0x79 && 
			 riid.Data4[5] == 0x5e && riid.Data4[6] == 0xdb && riid.Data4[7] == 0xcf;
	if (LogFile && d3d9device) fprintf(LogFile, "  d0223b96-bf7a-43fd-92bd-a43b0d82b9eb = IDirect3DDevice9\n");
	if (LogFile && d3d10device) fprintf(LogFile, "  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = ID3D10Device\n");
	if (LogFile && d3d10multithread) fprintf(LogFile, "  9b7e4e00-342c-4106-a19f-4f2704f689f0 = ID3D10Multithread\n");
	if (LogFile && dxgidevice) fprintf(LogFile, "  54ec77fa-1377-44e6-8c32-88fd5f44c84c = IDXGIDevice\n");
	if (LogFile && dxgidevice1) fprintf(LogFile, "  77db970f-6276-48ba-ba28-070143b4392c = IDXGIDevice1\n");
	if (LogFile && dxgidevice2) fprintf(LogFile, "  05008617-fbfd-4051-a790-144884b4f6a9 = IDXGIDevice2\n");
	/*
	if (LogFile && unknown1) fprintf(LogFile, "  7abb6563-02bc-47c4-8ef9-acc4795edbcf = undocumented. Forcing fail.\n");
	if (unknown1)
	{
		*ppvObj = 0;
		return E_OUTOFMEMORY;
	}
	*/
	HRESULT hr = m_pUnk->QueryInterface(riid, ppvObj);
	D3D11Wrapper::ID3D10Device *p4 = (D3D11Wrapper::ID3D10Device*) D3D11Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
    if (p4)
	{
		unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
		*ppvObj = p4;
		p4->AddRef();
		if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Device wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p4->m_ulRef);
	}
	D3D11Wrapper::ID3D10Multithread *p5 = (D3D11Wrapper::ID3D10Multithread*) D3D11Wrapper::ID3D10Multithread::m_List.GetDataPtr(*ppvObj);
    if (p5)
	{
		unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
		*ppvObj = p5;
		p5->AddRef();
		if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Multithread wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p5->m_ulRef);
	}
	if (!p4 && !p5)
	{
		// Check for DirectX10 cast.
		if (d3d10device)
		{
			D3D11Base::ID3D10Device *origDevice = (D3D11Base::ID3D10Device *)*ppvObj;
			D3D11Wrapper::ID3D10Device *wrapper = D3D11Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
			if(wrapper == NULL)
			{
				if (LogFile) fprintf(LogFile, "  error allocating ID3D10Device wrapper.\n");
				
				origDevice->Release();
				return E_OUTOFMEMORY;
			}
			*ppvObj = wrapper;
			if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Device wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
		}
		if (d3d10multithread)
		{
			D3D11Base::ID3D10Multithread *origDevice = (D3D11Base::ID3D10Multithread *)*ppvObj;
			D3D11Wrapper::ID3D10Multithread *wrapper = D3D11Wrapper::ID3D10Multithread::GetDirect3DMultithread(origDevice);
			if(wrapper == NULL)
			{
				if (LogFile) fprintf(LogFile, "  error allocating ID3D10Multithread wrapper.\n");
				
				origDevice->Release();
				return E_OUTOFMEMORY;
			}
			*ppvObj = wrapper;
			if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Multithread wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
		}
		// :todo: create d3d9 wrapper!
		if (d3d9device)
		{
			// create d3d9 wrapper!
		}
	}
	if (LogFile) fprintf(LogFile, "  result = %x, handle = %x\n", hr, *ppvObj);
	
	return hr;
}

// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
static UINT64 fnv_64_buf(const void *buf, size_t len)
{
	UINT64 hval = 0;
    unsigned const char *bp = (unsigned const char *)buf;	/* start of buffer */
    unsigned const char *be = bp + len;		/* beyond end of buffer */

    // FNV-1 hash each octet of the buffer
    while (bp < be) 
	{
		// multiply by the 64 bit FNV magic prime mod 2^64 */
		hval *= FNV_64_PRIME;
		// xor the bottom with the current octet
		hval ^= (UINT64)*bp++;
    }
	return hval;
}

#include "Direct3D10Device.h"
