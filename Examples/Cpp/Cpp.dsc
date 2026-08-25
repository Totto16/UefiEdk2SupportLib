[Defines]
  PLATFORM_NAME           = Example-Cpp
  PLATFORM_GUID           = 8fd29a57-a693-4e11-bb17-dc46086af564
  PLATFORM_VERSION        = 1.0
  DSC_SPECIFICATION       = 0x0001001B
  OUTPUT_DIRECTORY        = Build/Example/Cpp
  SUPPORTED_ARCHITECTURES = X64
  BUILD_TARGETS           = DEBUG|RELEASE
  SKUID_IDENTIFIER        = DEFAULT

  # FLASH_DEFINITION               = EmulatorPkg/EmulatorPkg.fdf


  #
  # Network definition
  #
  DEFINE NETWORK_SNP_ENABLE       = FALSE
  DEFINE NETWORK_IP6_ENABLE       = FALSE
  DEFINE NETWORK_TLS_ENABLE       = FALSE
  DEFINE NETWORK_HTTP_BOOT_ENABLE = FALSE
  DEFINE NETWORK_HTTP_ENABLE      = FALSE
  DEFINE NETWORK_ISCSI_ENABLE     = FALSE
  DEFINE SECURE_BOOT_ENABLE       = FALSE

  #
  # Redfish definition
  #
  DEFINE REDFISH_ENABLE = FALSE

  DEFINE DEBUG_ON_SERIAL_PORT = FALSE
  DEFINE DEBUG_TO_MEM = FALSE

!ifndef OOPETRIS_RUNTIME_TARGET
  !error "OOPETRIS_RUNTIME_TARGET must be set"
!endif

!if $(OOPETRIS_RUNTIME_TARGET) == "hardware"
  DEFINE PLAT_QEMU               = FALSE
  DEFINE QEMU_PV_VARS            = FALSE
  DEFINE DEBUG_ON_SERIAL_PORT    = FALSE
!elseif $(OOPETRIS_RUNTIME_TARGET) == "emulator"
  DEFINE PLAT_QEMU               = TRUE
  DEFINE QEMU_PV_VARS            = FALSE
  DEFINE DEBUG_ON_SERIAL_PORT    = TRUE
!else
  !error "OOPETRIS_RUNTIME_TARGET has invalid value"
!endif


[Packages]
  MdePkg/MdePkg.dec
  UefiCpuPkg/UefiCpuPkg.dec

#!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf

  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf


  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  StackCheckLib|MdePkg/Library/StackCheckLib/StackCheckLib.inf
  StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf



!include StdLib/StdLib.inc


[LibraryClasses.common.UEFI_APPLICATION]
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  #DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  #DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  #DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  #SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf
  MemDebugLogLib|OvmfPkg/Library/MemDebugLogLib/MemDebugLogLibNull.inf
  DebugLib|OvmfPkg/Library/PlatformDebugLibIoPort/PlatformDebugLibIoPort.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PlatformHookLib|MdeModulePkg/Library/BasePlatformHookLibNull/BasePlatformHookLibNull.inf
  PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf

  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf

  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf


!if $(OOPETRIS_RUNTIME_TARGET) == "hardware"
  #TODO: doesn't work on qemu, but maybe on CPU??
  TimerLib|UefiCpuPkg/Library/CpuTimerLib/BaseCpuTimerLib.inf
  LibUEfiSupport|SupportLib/Library/TimerLib/SupportLibTimerImpl.inf
!else
  LibUEfiSupportNanosleep|SupportLib/Library/Default/SupportLibDefaultNanosleep.inf

  LibUEfiSupportClock|SupportLib/Library/TimerLib/SupportLibTimerClock.inf
  #LibUEfiSupportClock|SupportLib/Library/Default/SupportLibDefaultClock.inf
  #LibUEfiSupportClock|SupportLib/Library/Null/SupportLibNullClock.inf

  ## doesn't work on qemu, setup (cpuid leaf 0x15) error
  ## TimerLib|UefiCpuPkg/Library/CpuTimerLib/BaseCpuTimerLib.inf

  ## not supported for DXE or UEFI_APPLICATION:
  ## TimerLib|OvmfPkg/Library/AcpiTimerLib/BaseAcpiTimerLib.inf

  ## NOT WORKING on some machines, works with q35, but not the default (pc?),
  ## the reason is Pmba reading is incorrect in the timerlib Constructor :(
  ## TimerLib|OvmfPkg/Library/AcpiTimerLib/DxeAcpiTimerLib.inf

  ## not working: need special emulator?
  ## TimerLib|EmulatorPkg/Library/DxeTimerLib/DxeTimerLib.inf

  TimerLib|OvmfPkg/Library/AcpiTimerLib/DxeAcpiTimerLib.inf

!endif

  ShellCEntryLibDynamical|SupportLib/Library/UefiShellCEntryLibDynamical/UefiShellCEntryLibDynamical.inf



[Components]
  Examples/Cpp/Cpp.inf

[PcdsFixedAtBuild]
#define DEBUG_WARN      0x00000002       // Warnings
#define DEBUG_INFO      0x00000040       // Informational debug messages
#define DEBUG_VERBOSE   0x00400000       // Detailed debug messages that may
#define DEBUG_ERROR     0x80000000       // Error
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80400042

# this is the same address as QEMU uses
  gUefiOvmfPkgTokenSpaceGuid.PcdDebugIoPort|0x402

#define DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED       0x01
#define DEBUG_PROPERTY_DEBUG_PRINT_ENABLED        0x02
#define DEBUG_PROPERTY_DEBUG_CODE_ENABLED         0x04
#define DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED       0x08
#define DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED  0x10
#define DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED    0x20

  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x3F

  # disable auto initialize, initialize manually, and if it fails, use backup non shell code backup
  gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE


!include LLVM/LLVMPkg.inc

