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

// GBACheats.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "GBACheats.h"

#include "../System.h"
#include "../Cheats.h"
#include "../CheatSearch.h"
#include "../GBA.h"
#include "../Globals.h"

#include "Reg.h"
#include "StringTokenizer.h"
#include "WinResUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// GBACheatSearch dialog

GBACheatSearch::GBACheatSearch(CWnd* pParent /*=NULL*/)
  : CDialog(GBACheatSearch::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBACheatSearch)
  valueType = -1;
  sizeType = -1;
  searchType = -1;
  numberType = -1;
  updateValues = FALSE;
  //}}AFX_DATA_INIT
  data = NULL;
}

GBACheatSearch::~GBACheatSearch()
{
  if(data)
    free(data);
}

void GBACheatSearch::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBACheatSearch)
  DDX_Control(pDX, IDC_VALUE, m_value);
  DDX_Control(pDX, IDC_CHEAT_LIST, m_list);
  DDX_Radio(pDX, IDC_OLD_VALUE, valueType);
  DDX_Radio(pDX, IDC_SIZE_8, sizeType);
  DDX_Radio(pDX, IDC_EQ, searchType);
  DDX_Radio(pDX, IDC_SIGNED, numberType);
  DDX_Check(pDX, IDC_UPDATE, updateValues);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GBACheatSearch, CDialog)
  //{{AFX_MSG_MAP(GBACheatSearch)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(IDC_START, OnStart)
  ON_BN_CLICKED(IDC_SEARCH, OnSearch)
  ON_BN_CLICKED(IDC_ADD_CHEAT, OnAddCheat)
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
// GBACheatSearch message handlers

void GBACheatSearch::OnOk() 
{
  EndDialog(TRUE);
}

void GBACheatSearch::OnStart() 
{
  if(cheatSearchData.count == 0) {
    CheatSearchBlock *block = &cheatSearchData.blocks[0];
    block->size = 0x40000;
    block->offset = 0x2000000;
    block->bits = (u8 *)malloc(0x40000>>3);
    block->data = workRAM;
    block->saved = (u8 *)malloc(0x40000);
    
    block = &cheatSearchData.blocks[1];
    block->size = 0x8000;
    block->offset = 0x3000000;
    block->bits = (u8 *)malloc(0x8000>>3);
    block->data = internalRAM;
    block->saved = (u8 *)malloc(0x8000);
    
    cheatSearchData.count = 2;
  }

  cheatSearchStart(&cheatSearchData);
  GetDlgItem(IDC_SEARCH)->EnableWindow(TRUE);
}

void GBACheatSearch::OnSearch() 
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

void GBACheatSearch::OnAddCheat() 
{
  int mark = m_list.GetSelectionMark();
  
  if(mark != -1) {
    LVITEM item;
    memset(&item,0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = mark;
    if(m_list.GetItem(&item)) {
      AddCheat dlg((u32)item.lParam);
      dlg.DoModal();
    }
  }
}

void GBACheatSearch::OnUpdate() 
{
  if(GetDlgItem(IDC_UPDATE)->SendMessage(BM_GETCHECK,
                                         0,
                                         0) & BST_CHECKED)
    updateValues = true;
  else
    updateValues = false;
  regSetDwordValue("cheatsUpdate", updateValues);
}

void GBACheatSearch::OnGetdispinfoCheatList(NMHDR* pNMHDR, LRESULT* pResult) 
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

void GBACheatSearch::OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult) 
{
  GetDlgItem(IDC_ADD_CHEAT)->EnableWindow(m_list.GetSelectionMark() != -1);
  *pResult = TRUE;
}

BOOL GBACheatSearch::OnInitDialog() 
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
  
  valueType = regQueryDwordValue("cheatsValueType", 0);
  if(valueType < 0 || valueType > 1)
    valueType = 0;

  searchType = regQueryDwordValue("cheatsSearchType", SEARCH_EQ);
  if(searchType > 5 || searchType < 0)
    searchType = 0;
  
  numberType = regQueryDwordValue("cheatsNumberType", 2);
  if(numberType < 0 || numberType > 2)
    numberType = 2;
  
  sizeType = regQueryDwordValue("cheatsSizeType", 0);
  if(sizeType < 0 || sizeType > 2)
    sizeType = 0;
  
  updateValues = regQueryDwordValue("cheatsUpdate", 0) ?
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

void GBACheatSearch::addChanges(bool showMsgs)
{
  int count = cheatSearchGetCount(&cheatSearchData, sizeType);
  
  m_list.DeleteAllItems();

  if(count > 1000) {
    if(showMsgs)
      systemMessage(IDS_SEARCH_PRODUCED_TOO_MANY,
                    "Search produced %d results. Please refine better",
                    count);
    return;
  }

  if(count == 0) {
    if(showMsgs)
      systemMessage(IDS_SEARCH_PRODUCED_NO_RESULTS,
                    "Search produced no results.");
    return;
  }
  
  m_list.SetItemCount(count);  
  if(data)
    free(data);
  
  data = (WinCheatsData *)calloc(count,sizeof(WinCheatsData));
  
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
                    block->offset | j,
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
                    block->offset | j,
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
    item.lParam = data[i].addr;
    item.state = 0;
    item.stateMask = 0;
    item.pszText = LPSTR_TEXTCALLBACK;
    m_list.InsertItem(&item);

    m_list.SetItemText(i, 1, LPSTR_TEXTCALLBACK);
    m_list.SetItemText(i, 2, LPSTR_TEXTCALLBACK);
  }  
}

void GBACheatSearch::addChange(int index, u32 address, u32 oldValue, u32 newValue)
{
  data[index].addr = address;
  sprintf(data[index].address, "%08x",address);
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

void GBACheatSearch::OnValueType(UINT id)
{
  switch(id) {
  case IDC_OLD_VALUE:
    valueType = 0;
    m_value.EnableWindow(FALSE);
    regSetDwordValue("cheatsValueType", 0);
    break;
  case IDC_SPECIFIC_VALUE:
    valueType = 1;
    m_value.EnableWindow(TRUE);
    regSetDwordValue("cheatsValueType", 1);     
    break;
  }
}

void GBACheatSearch::OnSearchType(UINT id)
{
  switch(id) {
  case IDC_EQ:
    searchType = SEARCH_EQ;
    regSetDwordValue("cheatsSearchType", 0);
    break;
  case IDC_NE:
    searchType = SEARCH_NE;
    regSetDwordValue("cheatsSearchType", 1);
    break;
  case IDC_LT:
    searchType = SEARCH_LT;
    regSetDwordValue("cheatsSearchType", 2);
    break;
  case IDC_LE:
    searchType = SEARCH_LE;
    regSetDwordValue("cheatsSearchType", 3);
    break;
  case IDC_GT:
    searchType = SEARCH_GT;
    regSetDwordValue("cheatsSearchType", 4);
    break;
  case IDC_GE:
    searchType = SEARCH_GE;
    regSetDwordValue("cheatsSearchType", 5);
    break;
  }
}

void GBACheatSearch::OnNumberType(UINT id)
{
  switch(id) {
  case IDC_SIGNED:
    numberType = 0;
    regSetDwordValue("cheatsNumberType", 0);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_UNSIGNED:
    numberType = 1;
    regSetDwordValue("cheatsNumberType", 1);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_HEXADECIMAL:
    numberType = 2;
    regSetDwordValue("cheatsNumberType", 2);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  }
}

void GBACheatSearch::OnSizeType(UINT id)
{
  switch(id) {
  case IDC_SIZE_8:
    sizeType = BITS_8;
    regSetDwordValue("cheatsSizeType", 0);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_SIZE_16:
    sizeType = BITS_16;
    regSetDwordValue("cheatsSizeType", 1);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  case IDC_SIZE_32:
    sizeType = BITS_32;
    regSetDwordValue("cheatsSizeType", 2);
    if(m_list.GetItemCount()) {
      addChanges(false);
    }
    break;
  }
}
/////////////////////////////////////////////////////////////////////////////
// AddCheat dialog


AddCheat::AddCheat(u32 address, CWnd* pParent /*=NULL*/)
  : CDialog(AddCheat::IDD, pParent)
{
  //{{AFX_DATA_INIT(AddCheat)
  sizeType = -1;
  numberType = -1;
  //}}AFX_DATA_INIT
  this->address = address;
}


void AddCheat::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AddCheat)
  DDX_Control(pDX, IDC_VALUE, m_value);
  DDX_Control(pDX, IDC_DESC, m_desc);
  DDX_Control(pDX, IDC_ADDRESS, m_address);
  DDX_Radio(pDX, IDC_SIZE_8, sizeType);
  DDX_Radio(pDX, IDC_SIGNED, numberType);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AddCheat, CDialog)
  //{{AFX_MSG_MAP(AddCheat)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_SIGNED, IDC_HEXADECIMAL, OnNumberType)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_SIZE_8, IDC_SIZE_32, OnSizeType)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AddCheat message handlers

void AddCheat::OnOk() 
{
  // add cheat
  if(addCheat()) {
    EndDialog(TRUE);
  }
}

void AddCheat::OnCancel() 
{
  EndDialog(FALSE);
}

BOOL AddCheat::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  if(address != 0) {
    CString buffer;
    buffer.Format("%08x", address);
    m_address.SetWindowText(buffer);
    m_address.EnableWindow(FALSE);
  }
  
  numberType = regQueryDwordValue("cheatsNumberType", 2);
  if(numberType < 0 || numberType > 2)
    numberType = 2;
  
  sizeType = regQueryDwordValue("cheatsSizeType", 0);
  if(sizeType < 0 || sizeType > 2)
    sizeType = 0;

  UpdateData(FALSE);
  
  GetDlgItem(IDC_DESC)->SendMessage(EM_LIMITTEXT,
                                    32,
                                    0);
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

void AddCheat::OnNumberType(UINT id)
{
  switch(id) {
  case IDC_SIGNED:
    numberType = 0;
    regSetDwordValue("cheatsNumberType", 0);
    break;
  case IDC_UNSIGNED:
    numberType = 1;
    regSetDwordValue("cheatsNumberType", 1);
    break;
  case IDC_HEXADECIMAL:
    numberType = 2;
    regSetDwordValue("cheatsNumberType", 2);
    break;
  }
}

void AddCheat::OnSizeType(UINT id)
{
  switch(id) {
  case IDC_SIZE_8:
    sizeType = BITS_8;
    regSetDwordValue("cheatsSizeType", 0);
    break;
  case IDC_SIZE_16:
    sizeType = BITS_16;
    regSetDwordValue("cheatsSizeType", 1);
    break;
  case IDC_SIZE_32:
    sizeType = BITS_32;
    regSetDwordValue("cheatsSizeType", 2);
    break;
  }
}

bool AddCheat::addCheat()
{
  CString buffer;
  CString code;

  m_address.GetWindowText(buffer);
  u32 address = 0;
  sscanf(buffer, "%x", &address);
  if((address >= 0x02000000 && address < 0x02040000) ||
     (address >= 0x03000000 && address < 0x03008000)) {
  } else {
    systemMessage(IDS_INVALID_ADDRESS, "Invalid address: %08x", address);
    return false;
  }
  if(sizeType != 0) {
    if(sizeType == 1 && address & 1) {
      systemMessage(IDS_MISALIGNED_HALFWORD,
                    "Misaligned half-word address: %08x", address);
      return false;
    }
    if(sizeType == 2 && address & 3) {
      systemMessage(IDS_MISALIGNED_WORD,
                    "Misaligned word address: %08x", address);
      return false;
    }    
  }
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

  switch(sizeType) {
  case 0:
    code.Format("%08x:%02x", address, value);
    break;
  case 1:
    code.Format("%08x:%04x", address, value);
    break;
  case 2:
    code.Format("%08x:%08x", address, value);
    break;
  }
  
  cheatsAdd(code, buffer, address ,address, value,-1, sizeType);
  return true;
}
/////////////////////////////////////////////////////////////////////////////
// GBACheatList dialog


GBACheatList::GBACheatList(CWnd* pParent /*=NULL*/)
  : CDialog(GBACheatList::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBACheatList)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  duringRefresh = false;
}


void GBACheatList::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBACheatList)
  DDX_Control(pDX, IDC_RESTORE, m_restore);
  DDX_Control(pDX, IDC_CHEAT_LIST, m_list);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GBACheatList, CDialog)
  //{{AFX_MSG_MAP(GBACheatList)
  ON_BN_CLICKED(IDC_ADD_CHEAT, OnAddCheat)
  ON_BN_CLICKED(IDC_ADD_CODE, OnAddCode)
  ON_BN_CLICKED(IDC_ADD_CODEBREAKER, OnAddCodebreaker)
  ON_BN_CLICKED(IDC_ADD_GAMESHARK, OnAddGameshark)
  ON_BN_CLICKED(IDC_ENABLE, OnEnable)
  ON_BN_CLICKED(IDC_REMOVE, OnRemove)
  ON_BN_CLICKED(IDC_REMOVE_ALL, OnRemoveAll)
  ON_BN_CLICKED(IDC_RESTORE, OnRestore)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_NOTIFY(LVN_ITEMCHANGED, IDC_CHEAT_LIST, OnItemchangedCheatList)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBACheatList message handlers

void GBACheatList::OnAddCheat() 
{
  AddCheat dlg(0);
  dlg.DoModal();
  refresh();
}

void GBACheatList::OnAddCode() 
{
  AddCheatCode dlg;
  dlg.DoModal();
  refresh();
}

void GBACheatList::OnAddCodebreaker() 
{
  AddCBACode dlg;
  dlg.DoModal();
  refresh();
}

void GBACheatList::OnAddGameshark() 
{
  AddGSACode dlg;
  dlg.DoModal();
  refresh();
}

void GBACheatList::OnEnable() 
{
  int mark = m_list.GetSelectionMark();
  int count = m_list.GetItemCount();
  
  if(mark != -1) {
    LVITEM item;
    for(int i = 0; i < count; i++) {
      memset(&item, 0, sizeof(item));
      item.mask = LVIF_PARAM|LVIF_STATE;
      item.stateMask = LVIS_SELECTED;
      item.iItem = i;
      if(m_list.GetItem(&item)) {
        if(item.state & LVIS_SELECTED) {
          if(cheatsList[item.lParam].enabled)
            cheatsDisable((int)(item.lParam & 0xFFFFFFFF));
          else
            cheatsEnable((int)(item.lParam & 0xFFFFFFFF));
        }
      }
    }
    refresh();
  }
}

void GBACheatList::OnRemove() 
{
  int mark = m_list.GetSelectionMark();
  int count = m_list.GetItemCount();
  
  if(mark != -1) {
    for(int i = count - 1; i >= 0; i--) {
      LVITEM item;
      memset(&item,0, sizeof(item));
      item.mask = LVIF_PARAM|LVIF_STATE;
      item.iItem = i;
      item.stateMask = LVIS_SELECTED;
      if(m_list.GetItem(&item)) {
        if(item.state & LVIS_SELECTED) {
          cheatsDelete((int)(item.lParam & 0xFFFFFFFF), restoreValues);
        }
      }
    }
    refresh();
  }         
}

void GBACheatList::OnRemoveAll() 
{
  cheatsDeleteAll(restoreValues);
  refresh();
}


void GBACheatList::OnRestore() 
{
  restoreValues = !restoreValues;
  regSetDwordValue("cheatsRestore", restoreValues);
}

void GBACheatList::OnOk() 
{
  EndDialog(TRUE);
}

void GBACheatList::OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult) 
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
          cheatsEnable((int)(l->lParam & 0xFFFFFFFF));
        else
          cheatsDisable((int)(l->lParam & 0xFFFFFFFF));
        refresh();
      }
    }
  }
  
  *pResult = 0;
}

BOOL GBACheatList::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CString temp = winResLoadString(IDS_CODE);
  m_list.InsertColumn(0, temp, LVCFMT_LEFT, 170, 0);
  temp = winResLoadString(IDS_DESCRIPTION);
  m_list.InsertColumn(1, temp, LVCFMT_LEFT, 150, 1);
  temp = winResLoadString(IDS_STATUS);
  m_list.InsertColumn(2, temp, LVCFMT_LEFT, 80, 1);
  
  m_list.SetFont(CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT)),
                 TRUE);

  m_list.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);  
  
  restoreValues = regQueryDwordValue("cheatsRestore", 0) ?
    true : false;
  
  m_restore.SetCheck(restoreValues);
  
  refresh();
  GetDlgItem(IDC_REMOVE)->EnableWindow(FALSE);
  GetDlgItem(IDC_ENABLE)->EnableWindow(FALSE);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBACheatList::refresh()
{
  duringRefresh = true;
  m_list.DeleteAllItems();
  
  CString buffer;
  
  for(int i = 0; i < cheatsNumber; i++) {
    LVITEM item;

    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
    item.iItem = i;
    item.iSubItem = 0;
    item.lParam = i;
    item.state = 0;
    item.stateMask = 0;
    item.pszText = cheatsList[i].codestring;
    m_list.InsertItem(&item);

    m_list.SetCheck(i, (cheatsList[i].enabled) ? TRUE : FALSE);

    m_list.SetItemText(i, 1, cheatsList[i].desc);
    
    buffer = (cheatsList[i].enabled) ? 'E' : 'D';    
    m_list.SetItemText(i, 2, buffer);
  }
  duringRefresh = false;
}
/////////////////////////////////////////////////////////////////////////////
// AddGSACode dialog


AddGSACode::AddGSACode(CWnd* pParent /*=NULL*/)
  : CDialog(AddGSACode::IDD, pParent)
{
  //{{AFX_DATA_INIT(AddGSACode)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void AddGSACode::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AddGSACode)
  DDX_Control(pDX, IDC_DESC, m_desc);
  DDX_Control(pDX, IDC_CODE, m_code);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AddGSACode, CDialog)
  //{{AFX_MSG_MAP(AddGSACode)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AddGSACode message handlers

void AddGSACode::OnOk() 
{
  CString desc;
  CString buffer;
  CString part1;
  CString code;
  CString token;  

  m_code.GetWindowText(buffer);
  m_desc.GetWindowText(desc);
  
  StringTokenizer st(buffer, " \t\n\r");
  part1.Empty();
  const char *t = st.next();
  while(t) {
    token = t;
    token.MakeUpper();
    if(token.GetLength() == 16)
      cheatsAddGSACode(token, desc, false);
    else if(token.GetLength() == 12) {
      code = token.Left(8);
      code += " ";
      code += token.Right(4);
      cheatsAddCBACode(code, desc);
    } else if(part1.IsEmpty())
      part1 = token;
    else {
      if(token.GetLength() == 4) {
        code = part1;
        code += " ";
        code += token;
        cheatsAddCBACode(code, desc);
      } else {
        code = part1 + token;
        cheatsAddGSACode(code, desc, true);
      }
      part1.Empty();
    }

    t = st.next();
  }
  EndDialog(TRUE);
}

void AddGSACode::OnCancel() 
{
  EndDialog(FALSE);
}

BOOL AddGSACode::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  m_code.LimitText(1024);
  m_desc.LimitText(32);
  CString title = winResLoadString(IDS_ADD_GSA_CODE);
  SetWindowText(title);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

/////////////////////////////////////////////////////////////////////////////
// AddCBACode dialog


AddCBACode::AddCBACode(CWnd* pParent /*=NULL*/)
  : CDialog(AddCBACode::IDD, pParent)
{
  //{{AFX_DATA_INIT(AddCBACode)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void AddCBACode::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AddCBACode)
  DDX_Control(pDX, IDC_DESC, m_desc);
  DDX_Control(pDX, IDC_CODE, m_code);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AddCBACode, CDialog)
  //{{AFX_MSG_MAP(AddCBACode)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AddCBACode message handlers

void AddCBACode::OnOk() 
{
  CString desc;
  CString buffer;
  CString part1;
  CString code;
  CString token;

  m_code.GetWindowText(buffer);
  m_desc.GetWindowText(desc);
  
  StringTokenizer st(buffer, " \t\n\r");
  part1.Empty();
  const char *t = st.next();
  while(t) {
    token = t;
    token.MakeUpper();
    if(token.GetLength() == 16)
      cheatsAddGSACode(token, desc, false);
    else if(token.GetLength() == 12) {
      code = token.Left(8);
      code += " ";
      code += token.Right(4);
      cheatsAddCBACode(code, desc);
    } else if(part1.IsEmpty())
      part1 = token;
    else {
      if(token.GetLength() == 4) {
        code = part1;
        code += " ";
        code += token;
        cheatsAddCBACode(code, desc);
      } else {
        code = part1 + token;
        cheatsAddGSACode(code, desc, true);
      }
      part1.Empty();
    }

    t = st.next();
  }
  EndDialog(TRUE);
}

void AddCBACode::OnCancel() 
{
  EndDialog(FALSE);
}

BOOL AddCBACode::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  m_code.LimitText(1024);
  m_desc.LimitText(32);
  CString title = winResLoadString(IDS_ADD_CBA_CODE);
  SetWindowText(title);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

/////////////////////////////////////////////////////////////////////////////
// AddCheatCode dialog


AddCheatCode::AddCheatCode(CWnd* pParent /*=NULL*/)
  : CDialog(AddCheatCode::IDD, pParent)
{
  //{{AFX_DATA_INIT(AddCheatCode)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void AddCheatCode::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AddCheatCode)
  DDX_Control(pDX, IDC_DESC, m_desc);
  DDX_Control(pDX, IDC_CODE, m_code);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AddCheatCode, CDialog)
  //{{AFX_MSG_MAP(AddCheatCode)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AddCheatCode message handlers

void AddCheatCode::OnOk() 
{
  CString desc;
  CString buffer;
  CString token;

  m_code.GetWindowText(buffer);
  m_desc.GetWindowText(desc);
  
  StringTokenizer st(buffer, " \t\n\r");
  const char *t = st.next();
  while(t) {
    token = t;
    token.MakeUpper();
    cheatsAddCheatCode(token, desc);
    t = st.next();
  }
  EndDialog(TRUE);
}

void AddCheatCode::OnCancel() 
{
  EndDialog(FALSE);
}

BOOL AddCheatCode::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  m_code.LimitText(1024);
  m_desc.LimitText(32);
  CString title = winResLoadString(IDS_ADD_CHEAT_CODE);
  SetWindowText(title);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
