//

// Copyright (c) 1997-2001 Microsoft Corporation, All Rights Reserved
//
#if !defined(AFX_VCDATAGRID_H__BD8F08FC_77F0_11D1_A9D5_0060081EBBAD__INCLUDED_)
#define AFX_VCDATAGRID_H__BD8F08FC_77F0_11D1_A9D5_0060081EBBAD__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// Machine generated IDispatch wrapper class(es) created by Microsoft Visual C++

// NOTE: Do not modify the contents of this file.  If this class is regenerated by
//  Microsoft Visual C++, your modifications will be overwritten.

/////////////////////////////////////////////////////////////////////////////
// CVcDataGrid wrapper class

class CVcDataGrid : public COleDispatchDriver
{
public:
	CVcDataGrid() {}		// Calls COleDispatchDriver default constructor
	CVcDataGrid(LPDISPATCH pDispatch) : COleDispatchDriver(pDispatch) {}
	CVcDataGrid(const CVcDataGrid& dispatchSrc) : COleDispatchDriver(dispatchSrc) {}

// Attributes
public:

// Operations
public:
	short GetColumnCount();
	void SetColumnCount(short nNewValue);
	short GetColumnLabelCount();
	void SetColumnLabelCount(short nNewValue);
	short GetRowLabelCount();
	void SetRowLabelCount(short nNewValue);
	short GetRowCount();
	void SetRowCount(short nNewValue);
	void DeleteColumns(short Column, short Count);
	void InsertColumns(short Column, short Count);
	void DeleteColumnLabels(short LabelIndex, short Count);
	void InsertColumnLabels(short LabelIndex, short Count);
	void DeleteRows(short Row, short Count);
	void InsertRows(short Row, short Count);
	void DeleteRowLabels(short LabelIndex, short Count);
	void InsertRowLabels(short LabelIndex, short Count);
	void RandomDataFill();
	void SetSize(short RowLabelCount, short ColumnLabelCount, short DataRowCount, short DataColumnCount);
	void InitializeLabels();
	void RandomFillColumns(short Column, short Count);
	void RandomFillRows(short Row, short Count);
	void MoveData(short Top, short Left, short Bottom, short Right, short OverOffset, short DownOffset);
	void GetData(short Row, short Column, double* DataPoint, short* nullFlag);
	void SetData(short Row, short Column, double DataPoint, short nullFlag);
	CString GetColumnLabel(short Column, short LabelIndex);
	void SetColumnLabel(short Column, short LabelIndex, LPCTSTR lpszNewValue);
	CString GetCompositeColumnLabel(short Column);
	CString GetCompositeRowLabel(short Row);
	CString GetRowLabel(short Row, short LabelIndex);
	void SetRowLabel(short Row, short LabelIndex, LPCTSTR lpszNewValue);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VCDATAGRID_H__BD8F08FC_77F0_11D1_A9D5_0060081EBBAD__INCLUDED_)
