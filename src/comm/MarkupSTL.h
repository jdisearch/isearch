/*
 * =====================================================================================
 *
 *       Filename:  MarkupSTL.h
 *
 *    Description:  MarkupSTL class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */



#if !defined(AFX_MARKUPSTL_H__948A2705_9E68_11D2_A0BF_00105A27C570__INCLUDED_)
#define AFX_MARKUPSTL_H__948A2705_9E68_11D2_A0BF_00105A27C570__INCLUDED_

#include <string>
#include <string.h>
#include <map>
#include <vector>

#ifdef _DEBUG
#define _DS(i) (i?&(m_strDoc.c_str())[m_aPos[i].nStartL]:0)
#define MARKUP_SETDEBUGSTATE m_pMainDS=_DS(m_iPos); m_pChildDS=_DS(m_iPosChild)
#else
#define MARKUP_SETDEBUGSTATE
#endif

class CMarkupSTL
{
public:
	CMarkupSTL() { SetDoc( NULL ); };
	CMarkupSTL( const char* szDoc ) { SetDoc( szDoc ); };
	CMarkupSTL( const CMarkupSTL& markup ) { *this = markup; };
	void operator=( const CMarkupSTL& markup );
	virtual ~CMarkupSTL() {};

	// Navigate
	bool Load( const char* szFileName );
	bool SetDoc( const char* szDoc );
	bool IsWellFormed();
	bool FindElem( const char* szName=NULL );
	bool FindChildElem( const char* szName=NULL );
	bool IntoElem();
	bool OutOfElem();
	void ResetChildPos() { x_SetPos(m_iPosParent,m_iPos,0); };
	
	void ResetMainPos() { x_SetPos(m_iPosParent,0,0); };
	void ResetPos() { x_SetPos(0,0,0); };
	std::string GetTagName() const;
	std::string GetChildTagName() const { return x_GetTagName(m_iPosChild); };
	std::string GetData() const { return x_GetData(m_iPos); };
	std::string GetChildData() const { return x_GetData(m_iPosChild); };
	//[CMARKUPDEV
	std::string FindGetData( const char* szName );
	//]CMARKUPDEV
	std::string GetAttrib( const char* szAttrib ) const { return x_GetAttrib(m_iPos,szAttrib); };
	std::string GetChildAttrib( const char* szAttrib ) const { return x_GetAttrib(m_iPosChild,szAttrib); };
	std::string GetAttribName( int n ) const;
	bool SavePos( const char* szPosName="" );
	bool RestorePos( const char* szPosName="" );
	bool GetOffsets( int& nStart, int& nEnd ) const;
	std::string GetError() const { return m_strError; };

	enum MarkupNodeType
	{
		MNT_ELEMENT					= 1,  // 0x01
		MNT_TEXT					= 2,  // 0x02
		MNT_WHITESPACE				= 4,  // 0x04
		MNT_CDATA_SECTION			= 8,  // 0x08
		MNT_PROCESSING_INSTRUCTION	= 16, // 0x10
		MNT_COMMENT					= 32, // 0x20
		MNT_DOCUMENT_TYPE			= 64, // 0x40
		MNT_EXCLUDE_WHITESPACE		= 123,// 0x7b
	};
	//[CMARKUPDEV
	int FindNode( int nType=0 );
	int GetNodeType() { return m_nNodeType; };
	bool AddNode( int nType, const char* szText ) { return x_AddNode(nType,szText,false); };
	bool InsertNode( int nType, const char* szText ) { return x_AddNode(nType,szText,true); };
	bool RemoveNode();
	//]CMARKUPDEV

	// Create
	bool Save( const char* szFileName );
	std::string GetDoc() const { return m_strDoc; };
	bool AddElem( const char* szName, const char* szData=NULL ) { return x_AddElem(szName,szData,false,false); };
	bool InsertElem( const char* szName, const char* szData=NULL ) { return x_AddElem(szName,szData,true,false); };
	bool AddChildElem( const char* szName, const char* szData=NULL ) { return x_AddElem(szName,szData,false,true); };
	bool InsertChildElem( const char* szName, const char* szData=NULL ) { return x_AddElem(szName,szData,true,true); };
	bool AddAttrib( const char* szAttrib, const char* szValue ) { return x_SetAttrib(m_iPos,szAttrib,szValue); };
	bool AddChildAttrib( const char* szAttrib, const char* szValue ) { return x_SetAttrib(m_iPosChild,szAttrib,szValue); };
	bool AddAttrib( const char* szAttrib, int nValue ) { return x_SetAttrib(m_iPos,szAttrib,nValue); };
	bool AddChildAttrib( const char* szAttrib, int nValue ) { return x_SetAttrib(m_iPosChild,szAttrib,nValue); };
	bool AddChildSubDoc( const char* szSubDoc ) { return x_AddSubDoc(szSubDoc,false,true); };
	bool InsertChildSubDoc( const char* szSubDoc ) { return x_AddSubDoc(szSubDoc,true,true); };
	std::string GetChildSubDoc() const;

	// Modify
	bool RemoveElem();
	bool RemoveChildElem();
	bool SetAttrib( const char* szAttrib, const char* szValue ) { return x_SetAttrib(m_iPos,szAttrib,szValue); };
	bool SetChildAttrib( const char* szAttrib, const char* szValue ) { return x_SetAttrib(m_iPosChild,szAttrib,szValue); };
	bool SetAttrib( const char* szAttrib, int nValue ) { return x_SetAttrib(m_iPos,szAttrib,nValue); };
	bool SetChildAttrib( const char* szAttrib, int nValue ) { return x_SetAttrib(m_iPosChild,szAttrib,nValue); };
	//[CMARKUPDEV
	bool RemoveAttrib( const char* szAttrib ) { return x_RemoveAttrib(m_iPos,szAttrib); };
	bool RemoveChildAttrib( const char* szAttrib ) { return x_RemoveAttrib(m_iPosChild,szAttrib); };
	bool FindSetData( const char* szName, const char* szData, int nCDATA=0 );
	//]CMARKUPDEV
	bool SetData( const char* szData, int nCDATA=0 ) { return x_SetData(m_iPos,szData,nCDATA); };
	bool SetChildData( const char* szData, int nCDATA=0 ) { return x_SetData(m_iPosChild,szData,nCDATA); };

	//[CMARKUPDEV
	// Base64
	static std::string EncodeBase64( const unsigned char* pBuffer, int nBufferLen );
	static int DecodeBase64( const std::string& strBase64, unsigned char* pBuffer, int nBufferLen );
	//]CMARKUPDEV

protected:

#ifdef _DEBUG
	const char* m_pMainDS;
	const char* m_pChildDS;
#endif

	std::string m_strDoc;
	std::string m_strError;

	struct ElemPos
	{
		ElemPos() { Clear(); };
		ElemPos( const ElemPos& pos ) { *this = pos; };
		bool IsEmptyElement() const { return (nStartR == nEndL + 1); };
		void Clear()
		{
			nStartL=0; nStartR=0; nEndL=0; nEndR=0; nReserved=0;
			iElemParent=0; iElemChild=0; iElemNext=0;
		};
		void AdjustStart( int n ) { nStartL+=n; nStartR+=n; };
		void AdjustEnd( int n ) { nEndL+=n; nEndR+=n; };
		int nStartL;
		int nStartR;
		int nEndL;
		int nEndR;
		int nReserved;
		int iElemParent;
		int iElemChild;
		int iElemNext;
	};

	typedef std::vector<ElemPos> vectorElemPosT;
	vectorElemPosT m_aPos;
	int m_iPosParent;
	int m_iPos;
	int m_iPosChild;
	int m_iPosFree;
	int m_nNodeType;
	//[CMARKUPDEV
	int m_nNodeOffset;
	int m_nNodeLength;
	//]CMARKUPDEV

	struct TokenPos
	{
		TokenPos( const char* sz ) { Clear(); szDoc = sz; };
		void Clear() { nL=0; nR=-1; nNext=0; bIsString=false; };
		bool Match( const char* szName )
		{
			int nLen = nR - nL + 1;
		// To ignore case, define MARKUP_IGNORECASE
		#ifdef MARKUP_IGNORECASE
			return ( (strnicmp( &szDoc[nL], szName, nLen ) == 0)
		#else
			return ( (strncmp( &szDoc[nL], szName, nLen ) == 0)
		#endif
				&& ( szName[nLen] == '\0' || strchr(" =/[",szName[nLen]) ) );
		};
		int nL;
		int nR;
		int nNext;
		const char* szDoc;
		bool bIsString;
	};

	struct SavedPos
	{
		int iPosParent;
		int iPos;
		int iPosChild;
	};
	typedef std::map<std::string,SavedPos> mapSavedPosT;
	mapSavedPosT m_mapSavedPos;

	void x_SetPos( int iPosParent, int iPos, int iPosChild )
	{
		m_iPosParent = iPosParent;
		m_iPos = iPos;
		m_iPosChild = iPosChild;
		m_nNodeType = iPos?MNT_ELEMENT:0;
		//[CMARKUPDEV
		m_nNodeOffset = 0;
		m_nNodeLength = 0;
		//]CMARKUPDEV
		MARKUP_SETDEBUGSTATE;
	};

	int x_GetFreePos();
	int x_ReleasePos();

	int x_ParseElem( int iPos );
	int x_ParseError( const char* szError, const char* szName = NULL );
	static bool x_FindChar( const char* szDoc, int& nChar, char c );
	static bool x_FindToken( TokenPos& token );
	std::string x_GetToken( const TokenPos& token ) const;
	int x_FindElem( int iPosParent, int iPos, const char* szPath );
	std::string x_GetTagName( int iPos ) const;
	std::string x_GetData( int iPos ) const;
	std::string x_GetAttrib( int iPos, const char* szAttrib ) const;
	bool x_AddElem( const char* szName, const char* szValue, bool bInsert, bool bAddChild );
	bool x_AddSubDoc( const char* szSubDoc, bool bInsert, bool bAddChild );
	bool x_FindAttrib( TokenPos& token, const char* szAttrib=NULL ) const;
	bool x_SetAttrib( int iPos, const char* szAttrib, const char* szValue );
	bool x_SetAttrib( int iPos, const char* szAttrib, int nValue );
	//[CMARKUPDEV
	bool x_RemoveAttrib( int iPos, const char* szAttrib );
	bool x_AddNode( int nNodeType, const char* szText, bool bInsert );
	void x_RemoveNode( int iPosParent, int& iPos, int& nNodeType, int& nNodeOffset, int& nNodeLength );
	void x_AdjustForNode( int iPosParent, int iPos, int nShift );
	//]CMARKUPDEV
	bool x_CreateNode( std::string& strNode, int nNodeType, const char* szText );
	void x_LocateNew( int iPosParent, int& iPosRel, int& nOffset, int nLength, int nFlags );
	int x_ParseNode( TokenPos& token );
	bool x_SetData( int iPos, const char* szData, int nCDATA );
	int x_RemoveElem( int iPos );
	void x_DocChange( int nLeft, int nReplace, const std::string& strInsert );
	void x_PosInsert( int iPos, int nInsertLength );
	void x_Adjust( int iPos, int nShift, bool bAfterPos = false );
	std::string x_TextToDoc( const char* szText, bool bAttrib = false ) const;
	std::string x_TextFromDoc( int nLeft, int nRight ) const;
};

#endif // !defined(AFX_MARKUPSTL_H__948A2705_9E68_11D2_A0BF_00105A27C570__INCLUDED_)
