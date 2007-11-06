// -*- C++ -*-
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

#ifndef __VBA_SIGCCOMPAT_H__
#define __VBA_SIGCCOMPAT_H__

#undef LIBSIGC_DISABLE_DEPRECATED
#include <sigc++/bind.h>
#include <sigc++/connection.h>

#include <sigc++/slot.h>
#include <sigc++/object.h>
#include <sigc++/functors/mem_fun.h>

namespace SigC
{

template <class T_return, class T_obj1, class T_obj2>
inline Slot0<T_return>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)() )
{ return ::sigc::bound_mem_functor0<T_return, T_obj2>
             (dynamic_cast< T_obj1&>(_A_obj), _A_func); }

template <class T_return, class T_arg1, class T_obj1, class T_obj2>
inline Slot1<T_return, T_arg1>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) )
{ return ::sigc::bound_mem_functor1<T_return, T_obj2, T_arg1>
             (dynamic_cast< T_obj1&>(_A_obj), _A_func); }

template <class T_return, class T_arg1,class T_arg2, class T_obj1, class T_obj2>
inline Slot2<T_return, T_arg1,T_arg2>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) )
{ return ::sigc::bound_mem_functor2<T_return, T_obj2, T_arg1,T_arg2>
             (dynamic_cast< T_obj1&>(_A_obj), _A_func); }

} // namespace SigC


#endif // __VBA_SIGCCOMPAT_H__
