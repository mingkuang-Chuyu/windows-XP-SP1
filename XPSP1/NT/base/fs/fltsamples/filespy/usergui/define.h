#pragma once

#define DEVICE_NAME_SIZE 64


#define IRP_NOCACHE                     0x00000001
#define IRP_PAGING_IO                   0x00000002
#define IRP_SYNCHRONOUS_API             0x00000004
#define IRP_SYNCHRONOUS_PAGING_IO       0x00000040

#define FS_FILTER_ACQUIRE_FOR_SECTION_SYNCHRONIZATION    (UCHAR)-1
#define FS_FILTER_RELEASE_FOR_SECTION_SYNCHRONIZATION    (UCHAR)-2
#define FS_FILTER_ACQUIRE_FOR_MOD_WRITE                  (UCHAR)-3
#define FS_FILTER_RELEASE_FOR_MOD_WRITE                  (UCHAR)-4
#define FS_FILTER_ACQUIRE_FOR_CC_FLUSH                   (UCHAR)-5
#define FS_FILTER_RELEASE_FOR_CC_FLUSH                   (UCHAR)-6

