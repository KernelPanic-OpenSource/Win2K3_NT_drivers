!if 0
   WARNING: Do not make changes to this file without first consulting
   with JSchwart.

   Any changes made without doing so will be summarily deleted.
!endif

_PROJECT_=Drivers

MSC_WARNING_LEVEL=/W3 /WX

PROJECT_BINDROP_DIR=oem\binary_release
!if "$(BINARY_PUBLISH_PROJECTS)"!="$(BINARY_PUBLISH_PROJECTS:Drivers=)"
BINPUBLISH=1
!endif

# for .mc files
MC_SOURCEDIR=$(O)

# for generated headers
PASS0_HEADERDIR=$(O)

# for RPC IDL files
PASS0_CLIENTDIR=$(O)
PASS0_SERVERDIR=$(O)

# for OLE IDL files
PASS0_SOURCEDIR=$(O)
MIDL_TLBDIR=$(O)

BUFFER_OVERFLOW_CHECKS=1
