/*++

Copyright (c) 1996 Microsoft Corporation

Module Name:

    growlist.c

Abstract:

    Simple buffer management functions that maintenence of a list of
    binary objects.

Author:

    08-Aug-1997   jimschm     Created

Revision History:

--*/

#include "pch.h"

#define INSERT_LAST     0xffffffff

PBYTE
pGrowListAdd (
    IN OUT  PGROWLIST GrowList,
    IN      UINT InsertBefore,
    IN      PBYTE DataToAdd,            OPTIONAL
    IN      UINT SizeOfData,
    IN      UINT NulBytesToAdd
    )

/*++

Routine Description:

  pGrowListAdd allocates memory for a binary block by using a pool, and
  then expands an array of pointers, maintaining a quick-access list.

Arguments:

  GrowList - Specifies the list to add the entry to

  InsertBefore - Specifies the index of the array element to insert
                 before, or INSERT_LIST to append.

  DataToAdd - Specifies the binary block of data to add.

  SizeOfData - Specifies the size of data.

  NulBytesToAdd - Specifies the number of nul bytes to add to the buffer

Return Value:

  A pointer to the binary block if data was copied into the list, 1 if a list
  item was created but no data was set for the item, or NULL if an error
  occurred.

--*/

{
    PBYTE *Item;
    PBYTE *InsertAt;
    PBYTE Data;
    UINT OldEnd;
    UINT Size;
    UINT TotalSize;

    TotalSize = SizeOfData + NulBytesToAdd;

    MYASSERT (TotalSize || !DataToAdd);

    //
    // Allocate pool if necessary
    //

    if (!GrowList->ListData) {
        GrowList->ListData = PoolMemInitNamedPool ("GrowList");
        if (!GrowList->ListData) {
            DEBUGMSG ((DBG_WARNING, "GrowList: Could not allocate pool"));
            return NULL;
        }

        PoolMemDisableTracking (GrowList->ListData);
    }

    //
    // Expand list array
    //

    OldEnd = GrowList->ListArray.End;
    Item = (PBYTE *) GrowBuffer (&GrowList->ListArray, sizeof (PBYTE));
    if (!Item) {
        DEBUGMSG ((DBG_WARNING, "GrowList: Could not allocate array item"));
        return NULL;
    }

    //
    // Copy data
    //

    if (DataToAdd || NulBytesToAdd) {
        Data = PoolMemGetAlignedMemory (GrowList->ListData, TotalSize);
        if (!Data) {
            GrowList->ListArray.End = OldEnd;
            DEBUGMSG ((DBG_WARNING, "GrowList: Could not allocate data block"));
            return NULL;
        }

        if (DataToAdd) {
            CopyMemory (Data, DataToAdd, SizeOfData);
        }
        if (NulBytesToAdd) {
            ZeroMemory (Data + SizeOfData, NulBytesToAdd);
        }
    } else {
        Data = NULL;
    }

    //
    // Adjust array
    //

    Size = GrowListGetSize (GrowList);

    if (InsertBefore >= Size) {
        //
        // Append mode
        //

        *Item = Data;

    } else {
        //
        // Insert mode
        //

        InsertAt = (PBYTE *) (GrowList->ListArray.Buf) + InsertBefore;
        MoveMemory (&InsertAt[1], InsertAt, (Size - InsertBefore) * sizeof (PBYTE));
        *InsertAt = Data;
    }

    return Data ? Data : (PBYTE) 1;
}


VOID
FreeGrowList (
    IN  PGROWLIST GrowList
    )

/*++

Routine Description:

  FreeGrowList frees the resources allocated by a GROWLIST.

Arguments:

  GrowList - Specifies the list to clean up

Return Value:

  none

--*/

{
    FreeGrowBuffer (&GrowList->ListArray);
    if (GrowList->ListData) {
        PoolMemDestroyPool (GrowList->ListData);
    }

    ZeroMemory (GrowList, sizeof (GROWLIST));
}


PBYTE
GrowListGetItem (
    IN      PGROWLIST GrowList,
    IN      UINT Index
    )

/*++

Routine Description:

  GrowListGetItem returns a pointer to the block of data
  for item specified by Index.

Arguments:

  GrowList - Specifies the list to access

  Index - Specifies zero-based index of item in list to access

Return Value:

  A pointer to the item's data, or NULL if the Index does not
  represent an actual item.

--*/

{
    PBYTE *ItemPtr;
    UINT Size;

    Size = GrowListGetSize (GrowList);
    if (Index >= Size) {
        return NULL;
    }

    ItemPtr = (PBYTE *) (GrowList->ListArray.Buf);
    MYASSERT(ItemPtr);

    return ItemPtr[Index];
}


UINT
GrowListGetSize (
    IN      PGROWLIST GrowList
    )

/*++

Routine Description:

  GrowListGetSize calculates the number of items in the list.

Arguments:

  GrowList - Specifies the list to calculate the size of

Return Value:

  The number of items in the list, or zero if the list is empty.

--*/

{
    return GrowList->ListArray.End / sizeof (PBYTE);
}


PBYTE
RealGrowListAppend (
    IN OUT  PGROWLIST GrowList,
    IN      PBYTE DataToAppend,         OPTIONAL
    IN      UINT SizeOfData
    )

/*++

Routine Description:

  GrowListAppend appends a black of data as a new list item.

Arguments:

  GrowList - Specifies the list to modify

  DataToAppend - Specifies a block of data to be copied

  SizeOfData - Specifies the number of bytes in DataToAppend

Return Value:

  A pointer to the binary block if data was copied into the list, 1 if a list
  item was created but no data was set for the item, or NULL if an error
  occurred.

--*/

{
    return pGrowListAdd (GrowList, INSERT_LAST, DataToAppend, SizeOfData, 0);
}


PBYTE
RealGrowListAppendAddNul (
    IN OUT  PGROWLIST GrowList,
    IN      PBYTE DataToAppend,         OPTIONAL
    IN      UINT SizeOfData
    )

/*++

Routine Description:

  GrowListAppend appends a black of data as a new list item and
  appends two zero bytes (used for string termination).

Arguments:

  GrowList - Specifies the list to modify

  DataToAppend - Specifies a block of data to be copied

  SizeOfData - Specifies the number of bytes in DataToAppend

Return Value:

  A pointer to the binary block if data was copied into the list, 1 if a list
  item was created but no data was set for the item, or NULL if an error
  occurred.

--*/

{
    return pGrowListAdd (GrowList, INSERT_LAST, DataToAppend, SizeOfData, 2);
}


PBYTE
RealGrowListInsert (
    IN OUT  PGROWLIST GrowList,
    IN      UINT Index,
    IN      PBYTE DataToInsert,         OPTIONAL
    IN      UINT SizeOfData
    )

/*++

Routine Description:

  GrowListAppend inserts a black of data as a new list item,
  before the specified Index.

Arguments:

  GrowList - Specifies the list to modify

  Index - Specifies the zero-based index of item to insert ahead of.

  DataToInsert - Specifies a block of data to be copied

  SizeOfData - Specifies the number of bytes in DataToInsert

Return Value:

  A pointer to the binary block if data was copied into the list, 1 if a list
  item was created but no data was set for the item, or NULL if an error
  occurred.

--*/

{
    UINT Size;

    Size = GrowListGetSize (GrowList);
    if (Index >= Size) {
        return NULL;
    }

    return pGrowListAdd (GrowList, Index, DataToInsert, SizeOfData, 0);
}


PBYTE
RealGrowListInsertAddNul (
    IN OUT  PGROWLIST GrowList,
    IN      UINT Index,
    IN      PBYTE DataToInsert,         OPTIONAL
    IN      UINT SizeOfData
    )

/*++

Routine Description:

  GrowListAppend inserts a block of data as a new list item,
  before the specified Index.  Two zero bytes are appended to
  the block of data (used for string termination).

Arguments:

  GrowList - Specifies the list to modify

  Index - Specifies the zero-based index of item to insert ahead of.

  DataToInsert - Specifies a block of data to be copied

  SizeOfData - Specifies the number of bytes in DataToInsert

Return Value:

  A pointer to the binary block if data was copied into the list, 1 if a list
  item was created but no data was set for the item, or NULL if an error
  occurred.

--*/

{
    UINT Size;

    Size = GrowListGetSize (GrowList);
    if (Index >= Size) {
        return NULL;
    }

    return pGrowListAdd (GrowList, Index, DataToInsert, SizeOfData, 2);
}


BOOL
GrowListDeleteItem (
    IN OUT  PGROWLIST GrowList,
    IN      UINT Index
    )

/*++

Routine Description:

  GrowListDeleteItem removes an item from the list.

Arguments:

  GrowList - Specifies the list to modify

  Index - Specifies the zero-based index of the item to remove.

Return Value:

  TRUE if the data block was removed from the list, or FALSE if
  Index is invalid.

--*/

{
    UINT Size;
    PBYTE *DeleteAt;

    Size = GrowListGetSize (GrowList);
    if (Size <= Index) {
        return FALSE;
    }

    DeleteAt = (PBYTE *) (GrowList->ListArray.Buf) + Index;
    if (*DeleteAt) {
        PoolMemReleaseMemory (GrowList->ListData, (PVOID) (*DeleteAt));
    }

    Size--;
    if (Size > Index) {
        MoveMemory (DeleteAt, &DeleteAt[1], (Size - Index) * sizeof (PBYTE));
    }

    GrowList->ListArray.End = Size * sizeof (PBYTE);

    return TRUE;
}


BOOL
GrowListResetItem (
    IN OUT  PGROWLIST GrowList,
    IN      UINT Index
    )

/*++

Routine Description:

  GrowListResetItem sets the list pointer of the specified item
  to NULL, freeing the memory associated with the item's data.

Arguments:

  GrowList - Specifies the list to modify

  Index - Specifies the zero-based index of the item to reset.

Return Value:

  TRUE if the data block was freed and the list element was nulled,
  or FALSE if Index is invalid.

--*/

{
    UINT Size;
    PBYTE *ResetAt;

    Size = GrowListGetSize (GrowList);
    if (Size <= Index) {
        return FALSE;
    }

    ResetAt = (PBYTE *) (GrowList->ListArray.Buf) + Index;
    if (*ResetAt) {
        PoolMemReleaseMemory (GrowList->ListData, (PVOID) (*ResetAt));
    }

    *ResetAt = NULL;

    return TRUE;
}


PBYTE
RealGrowListSetItem (
    IN OUT  PGROWLIST GrowList,
    IN      UINT Index,
    IN      PBYTE DataToCopy,
    IN      UINT DataSize
    )

/*++

Routine Description:

  GrowListSetItem replaces the data associated with a list item.

Arguments:

  GrowList - Specifies the list to modify

  Index - Specifies the zero-based index of the item to remove.

  DataToCopy - Specifies data to associate with the list item

  DataSize - Specifies the size of Data

Return Value:

  A pointer to the binary block if data was copied into the list, 1 if a list
  item was created but no data was set for the item, or NULL if an error
  occurred.

--*/

{
    UINT Size;
    PBYTE *ReplaceAt;
    PBYTE Data;

    MYASSERT (DataSize || !DataToCopy);

    Size = GrowListGetSize (GrowList);
    if (Size <= Index) {
        return NULL;
    }

    //
    // Copy data
    //

    if (DataToCopy) {
        Data = PoolMemGetAlignedMemory (GrowList->ListData, DataSize);
        if (!Data) {
            DEBUGMSG ((DBG_WARNING, "GrowList: Could not allocate data block (2)"));
            return NULL;
        }

        CopyMemory (Data, DataToCopy, DataSize);
    } else {
        Data = NULL;
    }

    //
    // Update list pointer
    //

    ReplaceAt = (PBYTE *) (GrowList->ListArray.Buf) + Index;
    if (*ReplaceAt) {
        PoolMemReleaseMemory (GrowList->ListData, (PVOID) (*ReplaceAt));
    }
    *ReplaceAt = Data;

    return Data ? Data : (PBYTE) 1;
}



