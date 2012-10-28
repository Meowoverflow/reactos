/* 
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * PURPOSE:          Interface to csrss
 * FILE:             subsys/win32k/ntuser/csr.c
 * PROGRAMER:        Ge van Geldorp (ge@gse.nl)
 */

#include <win32k.h>

static HANDLE WindowsApiPort = NULL;
PEPROCESS CsrProcess = NULL;

NTSTATUS FASTCALL
CsrInit(void)
{
   NTSTATUS Status;
   UNICODE_STRING PortName;
   ULONG ConnectInfoLength;
   SECURITY_QUALITY_OF_SERVICE Qos;   

   RtlInitUnicodeString(&PortName, L"\\Windows\\ApiPort");
   ConnectInfoLength = 0;
   Qos.Length = sizeof(Qos);
   Qos.ImpersonationLevel = SecurityDelegation;
   Qos.ContextTrackingMode = SECURITY_STATIC_TRACKING;
   Qos.EffectiveOnly = FALSE;

   Status = ZwConnectPort(&WindowsApiPort,
                          &PortName,
                          &Qos,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          &ConnectInfoLength);
   if (!NT_SUCCESS(Status))
   {
      return Status;
   }

   CsrProcess = PsGetCurrentProcess();

   return STATUS_SUCCESS;
}


NTSTATUS FASTCALL
co_CsrNotify(IN OUT PCSR_API_MESSAGE ApiMessage,
             IN ULONG DataLength)
{
    NTSTATUS Status;
    PEPROCESS OldProcess;

    if (NULL == CsrProcess)
    {
        return STATUS_INVALID_PORT_HANDLE;
    }

    /* Fill out the Port Message Header */
    ApiMessage->Header.u2.ZeroInit = 0;
    ApiMessage->Header.u1.s1.TotalLength =
        FIELD_OFFSET(CSR_API_MESSAGE, Data) + DataLength;
        /* FIELD_OFFSET(CSR_API_MESSAGE, Data) <= sizeof(CSR_API_MESSAGE) - sizeof(ApiMessage->Data) */
    ApiMessage->Header.u1.s1.DataLength =
        ApiMessage->Header.u1.s1.TotalLength - sizeof(PORT_MESSAGE);

    /* Switch to the process in which the WindowsApiPort handle is valid */
    OldProcess = PsGetCurrentProcess();
    if (CsrProcess != OldProcess)
    {
        KeAttachProcess(&CsrProcess->Pcb);
    }

    UserLeaveCo();

    Status = ZwRequestWaitReplyPort(WindowsApiPort,
                                    &ApiMessage->Header,
                                    &ApiMessage->Header);

    UserEnterCo();

    if (CsrProcess != OldProcess)
    {
        KeDetachProcess();
    }

    if (NT_SUCCESS(Status))
    {
        Status = ApiMessage->Status;
    }

    return Status;
}

/* EOF */
