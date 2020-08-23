#pragma once

#include "pe.hpp"
#include "mm.hpp"

#include "callbacks.hpp"
	
using namespace utils;

void load_image_callback( PUNICODE_STRING name, HANDLE pid, PIMAGE_INFO image_info )
{
	UNREFERENCED_PARAMETER( pid );

	const UNICODE_STRING drv_name = 
		RTL_CONSTANT_STRING( L"\\Device\\HarddiskVolume3\\Users\\G\\Desktop\\BadlionAnticheat.sys" );

	// forgot to add this check the first time lol
	if ( RtlCompareUnicodeString( name, &drv_name, FALSE ) )
		return;

	const auto base = reinterpret_cast< UINT64 >( image_info->ImageBase );
	const pe image( base );
	
	DbgPrint( "<< Init >> Image Callback Triggered: %wZ\nBase => %llx\n\n", name, base );

	auto desc = image.get_image_desc( );
	if ( nullptr == desc )
		return;

	DbgPrint( "<< Init >> Retrieving imports...\n" );
	
	while ( desc->Name != NULL ) 
	{
		STRING module_name;
		RtlInitAnsiString( &module_name, reinterpret_cast< PCSZ >( desc->Name + base ) );

		if ( const auto m_addr = pe::get_kernel_image( module_name ) ) 
		{
			DbgPrint( "\n<< Init >> Import Table Entry: %Z\nBase => %llx\n\n", module_name, m_addr );

			auto ori_first_thunk = reinterpret_cast< PIMAGE_THUNK_DATA64 >( base + desc->OriginalFirstThunk );
			auto first_thunk = reinterpret_cast< PIMAGE_THUNK_DATA64 >( base + desc->FirstThunk );

			while ( ori_first_thunk->u1.AddressOfData != NULL )
			{
				const auto p_fname = reinterpret_cast< PIMAGE_IMPORT_BY_NAME >( base + ori_first_thunk->u1.AddressOfData );
				
				STRING func_name;
				RtlInitAnsiString( &func_name, reinterpret_cast< PCSZ >( p_fname->Name ) );

				// if you're wondering why there is a big block of if-else statements it's because switch statements cannot be done on strings
				// and unfortunately I cannot use any stl map implementation because of kernel limitations AFAIK
				// I could implement hashing but this is sufficient 

				if ( !strcmp( func_name.Buffer, "ZwTerminateProcess" ) )
					place_hook( &first_thunk->u1.Function, ZwTerminateProcess_hk );

				else if ( !strcmp( func_name.Buffer, "MmGetSystemRoutineAddress" ) )
					place_hook( &first_thunk->u1.Function, MmGetSystemRoutineAddress_hk );

				else if ( !strcmp( func_name.Buffer, "PsSetCreateProcessNotifyRoutineEx" ) )
					place_hook( &first_thunk->u1.Function, PsSetCreateProcessNotifyRoutineEx_hk );

				else if ( !strcmp( func_name.Buffer, "PsSetLoadImageNotifyRoutine" ) )
					place_hook( &first_thunk->u1.Function, PsSetLoadImageNotifyRoutine_hk );

				else if ( !strcmp( func_name.Buffer, "RtlInitUnicodeString" ) )
					place_hook( &first_thunk->u1.Function, RtlInitUnicodeString_hk );

				else if ( !strcmp( func_name.Buffer, "PsLookupProcessByProcessId" ) )
					place_hook( &first_thunk->u1.Function, PsLookupProcessByProcessId_hk );

				else if ( !strcmp( func_name.Buffer, "IoCreateDevice" ) )
					place_hook( &first_thunk->u1.Function, IoCreateDevice_hk );

				else if ( !strcmp( func_name.Buffer, "ZwOpenProcess" ) )
					place_hook( &first_thunk->u1.Function, ZwOpenProcess_hk );

				
				++first_thunk;
				++ori_first_thunk;
			}
		}

		desc++;
	}
}