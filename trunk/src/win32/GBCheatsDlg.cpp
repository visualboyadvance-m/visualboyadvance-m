// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// GBCheats.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "GBCheatsDlg.h"
#include "Reg.h"
#include "StringTokenizer.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../CheatSearch.h"
#include "../gb/gbCheats.h"
#include "../gb/gbGlobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static bool winGbCheatAddVerifyGs(const char *code, const char *desc)
{
  gbAddGsCheat(code, desc);
  return true;
}

static bool winGbCheatAddVerifyGg(const char *code, const char *desc)
{
  gbAddGgCheat(code, desc);
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// GBCheatSearch dialog

GBCheatSearch::GBCheatSearch(CWnd* pParent /*=NULL*/)
  : CDialog(GBCheatSearch::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBCheatSearch)
  searchType = -1;
  numberType = -1;
  sizeType = -1;
  updateValues = FALSE;
  valueType = -1;
  //}}AFX_DATA_INIT
  data = NULL;
}

GBCheatSearch::~GBCheatSearch()
{
  if(data)
    free(data);
}

void GBCheatSearch::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBCheatSearch)
  DDX_Control(pDX, IDC_VALUE, m_value);
  DDX_Control(pDX, IDC_CHEAT_LIST, m_list);
  DDX_Radio(pDX, IDC_EQ, searchType);
  DDX_Radio(pDX, IDC_SIGNED, numberType);
  DDX_Radio(pDX, IDC_SIZE_8, sizeType);
  DDX_Check(pDX, IDC_UPDATE, updateValues);
  DDX_Radio(pDX, IDC_OLD_VALUE, valueType);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GBCheatSearch, CDialog)
  //{{AFX_MSG_MAP(GBCheatSearch)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(IDC_ADD_CHEAT, OnAddCheat)
  ON_BN_CLICKED(IDC_SEARCH, OnSearch)
  ON_BN_CLICKED(IDC_START, OnStart)
  ON_BN_CLICKED(IDC_UPDATE, OnUpdate)
  ON_NOTIFY(LVN_GETDISPINFO, IDC_CHEAT_LIST, OnGetdispinfoCheatList)
  ON_NOTIFY(LVN_ITEMCHANGED, IDC_CHEAT_LIST, OnItemchangedCheatList)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_OLD_VALUE, IDC_SPECIFIC_VALUE, OnValueType)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_EQ, IDC_GE, OnSearchType)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_SIGNED, IDC_HEXADECIMAL, OnNumberType)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_SIZE_8, IDC_SIZE_32, OnSizeType)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBCheatSearch message handlers

void GBCheatSearch::OnOk() 
{
  if(data)
    free(data);
  data = NULL;
  EndDialog(TRUE);
}

void GBCheatSearch::OnAddCheat() 
{
  int mark = m_list.GetSelectionMark();
  
  if(mark != -1) {
    LVITEM item;
    memset(&item,0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = mark;
    if(m_list.GetItem(&item)) {
      AddGBCheat dlg((u32)item.lParam);
      dlg.DoModal();
    }
  }
}

void GBCheatSearch::OnSearch() 
{
  CString buffer;
  if(valueType == 0)
    cheatSearch(&cheatSearchData,
		searchType,
		sizeType,
		numberType == 0);
  else {
    m_value.GetWindowText(buffer);
    if(buffer.IsEmpty()) {
      systemMessage(IDS_NUMBER_CANNOT_BE_EMPTY, "Number cannot be empty");
      return;
    }
    int value = 0;
    switch(numberType) {
    case 0:
      sscanf(buffer, "%d", &value);
      break;
    case 1:
      sscanf(buffer, "%u", &value);
      break;
    default:
      sscanf(buffer, "%x", &value);
    }
    cheatSearchValue(&cheatSearchData,
                     searchType,
                     sizeType,
                     numberType == 0,
                     value);
  }
  
  addChanges(true);

  if(updateValues)
    cheatSearchUpdateValues(&cheatSearchData);
}

void GBCheatSearch::OnStart() 
{
  if(cheatSearchData.count == 0) {
    int i = 0;

    CheatSearchBlock *block = &cheatSearchData.blocks[0];

    if(gbRamSize) {
      block->offset = 0xa000;
      if(gbRam)
	block->data = gbRam;
      else
	block->data = &gbMemory[0xa000];
      block->saved = (u8*)malloc(gbRamSize);
      block->size = gbRamSize;
      block->bits = (u8 *)malloc(gbRamSize >> 3);
      i++;
    }
    block = &cheatSearchData.blocks[i];
    if(gbCgbMode) {
      block->offset = 0xc000;
      block->data = &gbMemory[0xc000];
      block->saved = (u8*)malloc(0x1000);
      block->size = 0x1000;
      block->bits = (u8 *)malloc(0x1000 >> 3);
      i++;
      block =&cheatSearchData.blocks[i];
      block->offset = 0xd000;
      block->data = gbWram;
      block->saved = (u8*)malloc(0x8000);
      block->size = 0x8000;
      block->bits = (u8 *)malloc(0x8000 >> 3);
      i++;        
    } else {
      block->offset = 0xc000;
      block->data = &gbMemory[0xc000];
      block->saved = (u8*)malloc(0x2000);
      block->size = 0x2000;
      block->bits = (u8 *)malloc(0x2000 >> 3);
      i++;    
    }
    cheatSearchData.count = i;
  }

  cheatSearchStart(&cheatSearchData);
  GetDlgItem(IDC_SEARCH)->EnableWindow(TRUE);
}

void GBCheatSearch::OnUpdate() 
{
  if(GetDlgItem(IDC_UPDATE)->SendMessage(BM_GETCHECK,
                                         0,
                                         0) & BST_CHECKED)
    updateValues = true;
  else
    updateValues = false;
  regSetDwordValue("gbCheatsUpdate", updateValues);
}

BOOL GBCheatSearch::OnInitDialog() 
{
  CDialog::OnInitDialog();

  CString temp = winResLoadString(IDS_ADDRESS);

  m_list.InsertColumn(0, temp, LVCFMT_CENTER, 125, 0);

  temp = winResLoadString(IDS_OLD_VALUE);
  m_list.InsertColumn(1, temp, LVCFMT_CENTER, 125, 1);

  temp = winResLoadString(IDS_NEW_VALUE);
  m_list.InsertColumn(2, temp, LVCFMT_CENTER, 125, 2);
  
  m_list.SetFont(CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT)),
                 TRUE);

  m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
  
  if(!cheatSearchData.count) {
    GetDlgItem(IDC_SEARCH)->EnableWindow(FALSE);
    GetDlgItem(IDC_ADD_CHEAT)->EnableWindow(FALSE);
  }

  valueType = regQueryDwordValue("gbCheatsValueType", 0);
  if(valueType < 0 || valueType > 1)
    valueType = 2;
  
  searchType = regQueryDwordValue("gbCheatsSearchType",
                                  SEARCH_EQ);
  if(searchType < 0 || searchType > 5)
    searchType = 0;
  
  numberType = regQueryDwordValue("gbCheatsNumberType", 2);
  if(numberType < 0 || numberType > 2)
    numberType = 2;
  
  sizeType = regQueryDwordValue("gbCheatsSizeType", 0);
  if(sizeType < 0 || sizeType > 2)
    sizeType = 0;
  
  updateValues = regQueryDwordValue("gbCheatsUpdate", 0) ?
    true : false;
  
  UpdateData(FALSE);

  if(valueType == 0)
    m_value.EnableWindow(FALSE);
  
  CenterWindow();

  if(cheatSearchData.count) {
    addChanges(false);
  }

  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBCheatSearch::OnGetdispinfoCheatList(NMHDR* pNMHDR, LRESULT* pResult) 
{
  LV_DISPINFO* info = (LV_DISPINFO*)pNMHDR;
  if(info->item.mask & LVIF_TEXT) {
    int index = info->item.iItem;
    int col = info->item.iSubItem;
    
    switch(col) {
    case 0:
      strcpy(info->item.pszText, data[index].address);
      break;
    case 1:
      strcpy(info->item.pszText, data[index].oldValue);
      break;
    case 2:
      strcpy(info->item.pszText, data[index].newValue);
      break;
    }
  }
  *pResult = TRUE;
}

void GBCheatSearch::OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult) 
{
  GetDlgItem(IDC_ADD_CHEAT)->EnableWindow(m_list.GetSelectionMark() != -1);
}

int GBCheatSearch::getBank(u16 addr, int j)
{
  switch(addr >> 12) {
  case 0x0a:
    return j / 0x2000;
  case 0x0d:
    return j / 0x1000;
  }
  return 0;
}

void GBCheatSearch::addChange(int index, int bank, u16 address, int offset, u32 oldValue, u32 newValue)
{
  data[index].bank = bank;
  if(bank) {
    if(address == 0xa000)
      address |= offset & 0x1fff;
    else
      address |= offset & 0xfff;
  } else
    address |= offset;
  data[index].addr = address;
  sprintf(data[index].address, "%02x:%04x",bank,address);
  switch(numberType) {
  case 0:
    sprintf(data[index].oldValue, "%d", oldValue);
    sprintf(data[index].newValue, "%d", newValue);
    break;        
  case 1:
    sprintf(data[index].oldValue, "%u", oldValue);
    sprintf(data[index].newValue, "%u", newValue);
    break;    
  case 2:
    switch(sizeType) {
    case 0:
      sprintf(data[index].oldValue, "%02x", oldValue);
      sprintf(data[index].newValue, "%02x", newValue);      
      break;
    case 1:
      sprintf(data[index].oldValue, "%04x", oldValue);
      sprintf(data[index].newValue, "%04x", newValue);      
      break;
    case 2:
      sprintf(data[index].oldValue, "%08x", oldValue);
      sprintf(data[index].newValue, "%08x", newValue);      
      break;
    }
  }
}

void GBCheatSearch::addChanges(bool showMsg)
{
  int count = cheatSearchGetCount(&cheatSearchData, sizeType);
    
  m_list.DeleteAllItems();

  if(count > 1000) {
    if(showMsg)
      systemMessage(IDS_SEARCH_PRODUCED_TOO_MANY,
                    "Search produced %d results. Please refine better",
                    count);
    return;
  }  

  if(count == 0) {
    if(showMsg)
      systemMessage(IDS_SEARCH_PRODUCED_NO_RESULTS,
                    "Search produced no results");
    return;
  }
  
  m_list.SetItemCount(count);  
  if(data)
    free(data);

  data = (WinGbCheatsData *)calloc(count, sizeof(WinGbCheatsData));
  
  int inc = 1;
  switch(sizeType) {
  case 1:
    inc = 2;
    break;
  case 2:
    inc = 4;
    break;
  }
  
  int index = 0;
  if(numberType == 0) {
    for(int i = 0; i < cheatSearchData.count; i++) {
      CheatSearchBlock *block = &cheatSearchData.blocks[i];
      
      for(int j = 0; j < block->size; j+= inc) {
        if(IS_BIT_SET(block->bits, j)) {
          addChange(index++,
		    getBank(block->offset|j, j),
                    block->offset,
		    j,
                    cheatSearchSignedRead(block->saved,
                                          j,
                                          sizeType),
                    cheatSearchSignedRead(block->data,
                                          j,
                                          sizeType));
        }
      }
    }
  } else {
    for(int i = 0; i < cheatSearchData.count; i++) {
      CheatSearchBlock *block = &cheatSearchData.blocks[i];
      
      for(int j = 0; j < block->size; j+= inc) {
        if(IS_BIT_SET(block->bits, j)) {
          addChange(index++,
		    getBank(block->offset|j, j),
                    block->offset,
		    j,
                    cheatSearchRead(block->saved,
				    j,
				    sizeType),
                    cheatSearchRead(block->data,
				    j,
				    sizeType));
        }
      }
    }
  }

  for(int i = 0; i < count; i++) {
    LVITEM item;

    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
    item.iItem = i;
    item.iSubItem = 0;
    item.lParam = data[i].addr|
      (data[i].bank << 16);
    item.state = 0;
    item.stateMask = 0;
    item.pszText = LPSTR_TEXTCALLBACK;
    m_list.InsertItem(&item);

    m_list.SetItemText(i, 1, LPSTR_TEXTCALLBACK);
    m_list.SetItemText(i, 2, LPSTR_TEXTCALLBACK);
  }  
}

void GBCheatSearch::OnValueType(UINT id)
{
  switch(id) {
  case IDC_OLD_VALUE:
    valueType = 0;
    m_value.EnableWindow(FALSE);
    regSetDwordValue("gbCheatsValueType", 0);
    break;
  case IDC_SPECIFIC_VALUE:
    valueType = 1;
    m_value.EnableWindow(TRUE);
    regSetDwordValue("gbCheatsValueType", 1);   
    break;
  }
}

void GBCheatSearch::OnSearchType(UINT id)
{
  switch(id) {
  case IDC_EQ:
    searchType = SEARCH_EQ;
    regSetDwordValue("gbCheatsSearchType", 0);
    break;
  case IDC_NE:
    searchType = SEARCH_NE;
    regSetDwordValue("gbCheatsSearchType", 1);
    break;
  case IDC_LT:
    searchType = SEARCH_LT;
    regSetDwordValue("gbCheatsSearchType", 2);
    break;
  case IDC_LE:
    searchType = SEARCH_LE;
    regSetDwordValue("gbCheatsSearchType", 3);
    break;
  case IDC_GT:
    searchType = SEARCH_GT;
    regSetDwordValue("gbCheatsSearchType", 4);
    break;
  case IDC_GE:
    searchType = SEARCH_GE;
    regSetDwordValue("gbCheatsSearchType", 5);
    break;
  }
}

void GBCheatSearch::OnNumberType(UINT id)
{
  switch(id) {
  case IDC_SIGNED:
    numberType = 0;
    regSetDwordValue("gbCheatsNumberType", 0);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_UNSIGNED:
    numberType = 1;
    regSetDwordValue("gbCheatsNumberType", 1);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_HEXADECIMAL:
    numberType = 2;
    regSetDwordValue("gbCheatsNumberType", 2);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  }
}

void GBCheatSearch::OnSizeType(UINT id)
{
  switch(id) {
  case IDC_SIZE_8:
    sizeType = BITS_8;
    regSetDwordValue("gbCheatsSizeType", 0);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_SIZE_16:
    sizeType = BITS_16;
    regSetDwordValue("gbCheatsSizeType", 1);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_SIZE_32:
    sizeType = BITS_32;
    regSetDwordValue("gbCheatsSizeType", 2);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  }
}
/////////////////////////////////////////////////////////////////////////////
// AddGBCheat dialog


AddGBCheat::AddGBCheat(u32 addr, CWnd* pParent /*=NULL*/)
  : CDialog(AddGBCheat::IDD, pParent)
{
  //{{AFX_DATA_INIT(AddGBCheat)
  sizeType = -1;
  numberType = -1;
  //}}AFX_DATA_INIT
  address = addr;
}


void AddGBCheat::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AddGBCheat)
  DDX_Control(pDX, IDC_VALUE, m_value);
  DDX_Control(pDX, IDC_ADDRESS, m_address);
  DDX_Control(pDX, IDC_DESC, m_desc);
  DDX_Radio(pDX, IDC_SIZE_8, sizeType);
  DDX_Radio(pDX, IDC_SIGNED, numberType);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AddGBCheat, CDialog)
  //{{AFX_MSG_MAP(AddGBCheat)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_SIGNED, IDC_HEXADECIMAL, OnNumberType)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_SIZE_8, IDC_SIZE_32, OnSizeType)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AddGBCheat message handlers

void AddGBCheat::OnCancel() 
{
  EndDialog(FALSE);
}

void AddGBCheat::OnOk() 
{
  // add cheat
  if(addCheat()) {
    EndDialog(TRUE);
  }
}

bool AddGBCheat::addCheat()
{
  CString buffer;
  CString code;

  u32 value;
  m_value.GetWindowText(buffer);
  
  if(buffer.IsEmpty()) {
    systemMessage(IDS_VALUE_CANNOT_BE_EMPTY, "Value cannot be empty");
    return false;
  }
  
  switch(numberType) {
  case 0:
    sscanf(buffer, "%d", &value);
    break;
  case 1:
    sscanf(buffer, "%u", &value);
    break;
  default:
    sscanf(buffer, "%x", &value);
  }

  m_desc.GetWindowText(buffer);

  LONG_PTR bank = (address >> 16);
  address &= 0xFFFF;

  if(address >= 0xd000)
    bank += 0x90;
  else
    bank = 0x01;
  
  switch(sizeType) {
  case 0:
    code.Format("%02X%02X%02X%02X", bank, value, address&0xFF, address>>8);
    gbAddGsCheat(code, buffer);
    break;
  case 1:
    code.Format("%02X%02X%02X%02X", bank, value&0xFF, address&0xFF,
                address>>8);
    gbAddGsCheat(code, buffer);
    address++;
    code.Format("%02X%02X%02X%02X", bank, value>>8, address&0xFF,
                address>>8);
    gbAddGsCheat(code, buffer);    
    break;
  case 2:
    code.Format("%02X%02X%02X%02X", bank, value&0xFF, address&0xFF,
                address>>8);
    gbAddGsCheat(code, buffer);
    address++;
    code.Format("%02X%02X%02X%02X", bank, (value>>8) & 0xFF, address&0xFF,
                address>>8);
    gbAddGsCheat(code, buffer);
    address++;
    code.Format("%02X%02X%02X%02X", bank, (value>>16)&0xFF, address&0xFF,
                address>>8);
    gbAddGsCheat(code, buffer);
    address++;
    code.Format("%02X%02X%02X%02X", bank, value>>24, address&0xFF,
                address>>8);
    gbAddGsCheat(code, buffer);    
    break;
  }
  
  return true;
}

BOOL AddGBCheat::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CString buffer;
  buffer.Format("%02x:%08x", (address>>16), address&0xFFFF);
  m_address.SetWindowText(buffer);
  m_address.EnableWindow(FALSE);
  ::SetWindowLongPtr( m_address.GetSafeHwnd(), GWLP_USERDATA, address);
  
  numberType = regQueryDwordValue("gbCheatsNumberType", 2);
  if(numberType < 0 || numberType > 2)
    numberType = 2;
  
  sizeType = regQueryDwordValue("gbCheatsSizeType", 0);
  if(sizeType < 0 || sizeType > 2)
    sizeType = 0;

  UpdateData(FALSE);
  
  m_desc.LimitText(32);
  
  if(address != 0) {
    GetDlgItem(IDC_SIZE_8)->EnableWindow(FALSE);
    GetDlgItem(IDC_SIZE_16)->EnableWindow(FALSE);
    GetDlgItem(IDC_SIZE_32)->EnableWindow(FALSE);
    GetDlgItem(IDC_HEXADECIMAL)->EnableWindow(FALSE);
    GetDlgItem(IDC_UNSIGNED)->EnableWindow(FALSE);
    GetDlgItem(IDC_SIGNED)->EnableWindow(FALSE);        
  }
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void AddGBCheat::OnNumberType(UINT id)
{
  switch(id) {
  case IDC_SIGNED:
    numberType = 0;
    regSetDwordValue("gbCheatsNumberType", 0);
    break;
  case IDC_UNSIGNED:
    numberType = 1;
    regSetDwordValue("gbCheatsNumberType", 1);
    break;
  case IDC_HEXADECIMAL:
    numberType = 2;
    regSetDwordValue("gbCheatsNumberType", 2);
    break;
  }
}

void AddGBCheat::OnSizeType(UINT id)
{
  switch(id) {
  case IDC_SIZE_8:
    sizeType = BITS_8;
    regSetDwordValue("gbCheatsSizeType", 0);
    break;
  case IDC_SIZE_16:
    sizeType = BITS_16;
    regSetDwordValue("gbCheatsSizeType", 1);
    break;
  case IDC_SIZE_32:
    sizeType = BITS_32;
    regSetDwordValue("gbCheatsSizeType", 2);
    break;
  }
}

/////////////////////////////////////////////////////////////////////////////
// GBCheatList dialog


GBCheatList::GBCheatList(CWnd* pParent /*=NULL*/)
  : CDialog(GBCheatList::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBCheatList)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  duringRefresh = false;
}


void GBCheatList::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBCheatList)
  DDX_Control(pDX, IDC_CHEAT_LIST, m_list);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GBCheatList, CDialog)
  //{{AFX_MSG_MAP(GBCheatList)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(IDC_ADD_GG_CHEAT, OnAddGgCheat)
  ON_BN_CLICKED(IDC_ADD_GS_CHEAT, OnAddGsCheat)
  ON_BN_CLICKED(IDC_ENABLE, OnEnable)
  ON_BN_CLICKED(IDC_REMOVE, OnRemove)
  ON_BN_CLICKED(IDC_REMOVE_ALL, OnRemoveAll)
  ON_NOTIFY(LVN_ITEMCHANGED, IDC_CHEAT_LIST, OnItemchangedCheatList)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBCheatList message handlers

void GBCheatList::OnOk() 
{
  EndDialog(TRUE);
}

void GBCheatList::OnAddGgCheat() 
{
  CString temp = winResLoadString(IDS_ADD_GG_CODE);
  AddGBCode dlg(winGbCheatAddVerifyGg, 11, temp);
  dlg.DoModal();
  refresh();
}

void GBCheatList::OnAddGsCheat() 
{
  CString temp = winResLoadString(IDS_ADD_GS_CODE);

  AddGBCode dlg(winGbCheatAddVerifyGs, 8, temp);
  dlg.DoModal();
  refresh();
}

void GBCheatList::OnEnable() 
{
  int mark = m_list.GetSelectionMark();
  
  if(mark != -1) {
    LVITEM item;
    memset(&item,0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = mark;
    if(m_list.GetItem(&item)) {
      if(gbCheatList[item.lParam].enabled)
        gbCheatDisable((int)item.lParam);
      else
        gbCheatEnable((int)item.lParam);
      refresh();
    }       
  }
}

void GBCheatList::OnRemove() 
{
  int mark = m_list.GetSelectionMark();
  
  if(mark != -1) {
    LVITEM item;
    memset(&item,0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = mark;
    if(m_list.GetItem(&item)) {
      gbCheatRemove((int)item.lParam);
      refresh();
    }       
  }
}

void GBCheatList::OnRemoveAll() 
{
  gbCheatRemoveAll();
  refresh();
}

void GBCheatList::OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult) 
{
  if(m_list.GetSelectionMark() != -1) {
    GetDlgItem(IDC_REMOVE)->EnableWindow(TRUE);
    GetDlgItem(IDC_ENABLE)->EnableWindow(TRUE);
  } else {
    GetDlgItem(IDC_REMOVE)->EnableWindow(FALSE);
    GetDlgItem(IDC_ENABLE)->EnableWindow(FALSE);
  }
  
  if(!duringRefresh) {
    LPNMLISTVIEW l = (LPNMLISTVIEW)pNMHDR;
    if(l->uChanged & LVIF_STATE) {
      if(((l->uOldState & LVIS_STATEIMAGEMASK)>>12) !=
         (((l->uNewState & LVIS_STATEIMAGEMASK)>>12))) {
        if(m_list.GetCheck(l->iItem))
          gbCheatEnable((int)l->lParam);
        else
          gbCheatDisable((int)l->lParam);
        refresh();
      }
    }
  }    
}

BOOL GBCheatList::OnInitDialog() 
{
  CDialog::OnInitDialog();

  CString temp = winResLoadString(IDS_CODE);
  m_list.InsertColumn(0, temp, LVCFMT_LEFT, 120, 0);
  temp = winResLoadString(IDS_DESCRIPTION);
  m_list.InsertColumn(1, temp, LVCFMT_LEFT, 200, 1);
  temp = winResLoadString(IDS_STATUS);
  m_list.InsertColumn(2, temp, LVCFMT_LEFT, 80, 2);
  
  m_list.SetFont(CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT)),
                 TRUE);

  m_list.SetExtendedStyle(LVS_EX_CHECKBOXES |
                          LVS_EX_FULLROWSELECT);
  
  refresh();
  GetDlgItem(IDC_REMOVE)->EnableWindow(FALSE);
  GetDlgItem(IDC_ENABLE)->EnableWindow(FALSE);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBCheatList::refresh()
{
  duringRefresh = true;
  
  m_list.DeleteAllItems();
  
  char buffer[2];
  
  for(int i = 0; i < gbCheatNumber; i++) {
    LVITEM item;
    
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
    item.iItem = i;
    item.iSubItem = 0;
    item.lParam = i;
    item.state = 0;
    item.stateMask = 0;
    item.pszText = gbCheatList[i].cheatCode;
    m_list.InsertItem(&item);

    m_list.SetCheck(i, (gbCheatList[i].enabled ? TRUE : FALSE));
    
    m_list.SetItemText(i, 1, gbCheatList[i].cheatDesc);
    
    buffer[0] = (gbCheatList[i].enabled) ? 'E' : 'D';    
    buffer[1] = 0;
    m_list.SetItemText(i, 2, buffer);
  }
  duringRefresh = false;
}

/////////////////////////////////////////////////////////////////////////////
// AddGBCode dialog


AddGBCode::AddGBCode(bool (*verify)(const char *,const char*), int len, const char *title, CWnd* pParent /*=NULL*/)
  : CDialog(AddGBCode::IDD, pParent)
{
  //{{AFX_DATA_INIT(AddGBCode)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  addVerify = verify;
  addLength = len;
  addTitle = title;
}


void AddGBCode::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AddGBCode)
  DDX_Control(pDX, IDC_DESC, m_desc);
  DDX_Control(pDX, IDC_CODE, m_code);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AddGBCode, CDialog)
  //{{AFX_MSG_MAP(AddGBCode)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AddGBCode message handlers

void AddGBCode::OnOk() 
{
  CString desc;
  CString buffer;
  m_code.GetWindowText(buffer);
  m_desc.GetWindowText(desc);
  
  StringTokenizer st(buffer, " \t\n\r");
  const char *t = st.next();
  while(t) {
    addVerify(t, desc);
    t = st.next();
  }
  EndDialog(TRUE);
}

void AddGBCode::OnCancel() 
{
  EndDialog(FALSE);
}

BOOL AddGBCode::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  m_code.LimitText(1024);
  m_desc.LimitText(32);
  SetWindowText(addTitle);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
