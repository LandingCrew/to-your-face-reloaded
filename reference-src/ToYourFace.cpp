#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"
#include "skse64_common/Relocation.h"
#include "xbyak/xbyak.h"
#include "skse64/GameReferences.h"

#include <ShlObj.h>

const char* kConfigFile = "Data\\SKSE\\Plugins\\to_your_face.ini";



// F3 0F 59 F6 0F B6 EB B8 01 00 00 00 0F 2F F0 0F 43 E8
const UInt8  kCommentBytes[] = {
	0xF3, 0x0F, 0x59, 0xF6, 		// mulss xmm6,xmm6		
	0x0F, 0xB6, 0xEB, 				// movzx ebp,bl
	0xB8, 0x01, 0x00, 0x00, 0x00, 	// mov eax,1			
	0x0F, 0x2F, 0xF0, 				// comiss xmm6,xmm0
	0x0F, 0x43, 0xE8				// cmovae ebp,eax
};
const UInt32 kCommentByteCount = sizeof(kCommentBytes);


uintptr_t GetCommentAddress()
{
	uintptr_t start = RelocationManager::s_baseAddr + 0x1000;
	for (; start < start + 0x01000000; ++start)
	{
		if (!memcmp((void*)start, kCommentBytes, kCommentByteCount))
		{
			return start;
		}
	}
	return 0;
}

// 1.5.3 = 0x0065D1C7
// 1.5.16 = 0x0065E677
const uintptr_t kCommentAddress = GetCommentAddress(); // RelocationManager::s_baseAddr + 0x0065E677;

const UInt8 kAsm8Nop = 0x90;

const float pi = 3.14159265;


float allowedDeviationAngle = 30.0 / 180.0 * pi;

// At least 12 bytes required for this jump
void WriteLongJmp64(void* source, void* destination, DWORD64 length) {
	DWORD dwOld;
	//mov rax,0xABABABABABABABAB
	//jmp rax

	ASSERT(length >= 0xC);
	BYTE payload[] = { 
						0x48, 0xB8, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 	//mov rax,0xABABABABABABABAB
						0xFF, 0xE0														//jmp rax
					 };

	VirtualProtect(source, length, PAGE_EXECUTE_READWRITE, &dwOld);

	*(void**)(payload + 2) = destination;
	memcpy(source, payload, 12);
	memset((BYTE*)source + 12, 0x90, length - 12);

	VirtualProtect(source, length, dwOld, &dwOld);
}

bool AllowComment(Character* npc) {
	if (!npc || npc == *g_thePlayer) {
		return true;
	}

	float dx = npc->pos.x - (*g_thePlayer)->pos.x;
	float dy = npc->pos.y - (*g_thePlayer)->pos.y;
	float angle = atan2(dx, dy);  // x,y: clockwise; 0 at the top
	if (angle < 0.0) {
		angle += pi * 2.0;
	}
	float deviation = abs(angle - (*g_thePlayer)->rot.z);
	if (deviation > pi) {
		deviation = 2 * pi - deviation;
	}
	return deviation < allowedDeviationAngle;
}

bool IsBinaryCompatible() {
	return !memcmp((void*)kCommentAddress, kCommentBytes, kCommentByteCount);
}


void WriteCommentHook()
{
	static UInt8* midfnbuffer;
	struct CommentHookMidFunc_code : Xbyak::CodeGenerator
	{
		CommentHookMidFunc_code(void * buf) : Xbyak::CodeGenerator(0x100, buf)
		{
			Xbyak::Label midfn_end;

			mulss(xmm6, xmm6);
			xor(ebp, ebp);
			mov(eax, 1);
			comiss(xmm0, xmm6);
			jae(midfn_end);						// distance from player > fAIMinGreetingDistance

			push(rax);						    // rax pushed twice to keep 16 byte alignment as per x64 calling convention
			push(rax);
			push(rcx);
			push(rdx);

			mov(rcx, rdi);
			mov(rax, (uintptr_t)AllowComment);
			call(rax);

			test(al, al);					  

			pop(rdx);
			pop(rcx);
			pop(rax);
			pop(rax);

			setnz(bpl);

			L(midfn_end);
			push(rax);
			mov(rax, kCommentAddress + kCommentByteCount);
			xchg(rax, ptr[rsp]);
			ret();
		}
	};
	DWORD prot;
	midfnbuffer = (UInt8*)malloc(0x100);
	VirtualProtect(midfnbuffer, 0x100, PAGE_EXECUTE_READWRITE, &prot);
	CommentHookMidFunc_code code(midfnbuffer);

	WriteLongJmp64((void*)kCommentAddress, (void*)code.getCode(), kCommentByteCount);
}

extern "C"
{

	__declspec(dllexport) bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS,
			"\\My Games\\Skyrim Special Edition\\SKSE\\to_your_face.log");
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		_MESSAGE("query");

		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "to_your_face_sse";
		info->version = 1;

		if (skse->isEditor) {
			_MESSAGE("loaded in editor, marking as incompatible");

			return false;
		}
#if 0 // now designed to scan for function so no updates required
		else if (skse->runtimeVersion != RUNTIME_VERSION_1_5_16) {
			_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);

			return false;
		}
#endif
		else  if (!kCommentAddress || !IsBinaryCompatible()) {
			_MESSAGE("skyrim binary incompatible or incompatible skse plugin loaded");

			return false;
		}

		// supported runtime version
		return true;
	}



	__declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface * skse)
	{
		_MESSAGE("load");

		if (!IsBinaryCompatible()) {
			_MESSAGE("ERROR: incompatible SKSE plugin loaded!");

			return false;
		}

		allowedDeviationAngle = GetPrivateProfileInt("Main", "MaxDeviationAngle", 30, kConfigFile) / 180.0 * pi;
		_MESSAGE("MaxDeviationAngle: %.0f", round(allowedDeviationAngle * 180.0 / pi));

		WriteCommentHook();

		_MESSAGE("done");

		return true;
	}

};