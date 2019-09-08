#!/usr/bin/python
#===-- armv8_qemu_target_definition.py ------------------*- C++ -*-===//
#
#                     The LLVM Compiler Infrastructure
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//

#----------------------------------------------------------------------
# DESCRIPTION
#
# This file can be used with the following setting:
#   plugin.process.gdb-remote.target-definition-file
# This setting should be used when you are trying to connect to a 
# remote GDB server that doesn't support any of the register discovery
# packets that LLDB normally uses. 
#
# Why is this necessary? LLDB doesn't require a new build of LLDB that
# targets each new architecture you will debug with. Instead, all
# architectures are supported and LLDB relies on extra GDB server 
# packets to discover the target we are connecting to so that is can
# show the right registers for each target. This allows the GDB server
# to change and add new registers without requiring a new LLDB build
# just so we can see new registers.
#
# This file implements the x86_64 registers for the darwin version of
# GDB and allows you to connect to servers that use this register set. 
# 
# USAGE
#
# (lldb) settings set plugin.process.gdb-remote.target-definition-file /path/to/armv8_qemu_target_defintion.py
# (lldb) gdb-remote other.baz.com:1234
#
# The target definition file will get used if and only if the 
# qRegisterInfo packets are not supported when connecting to a remote
# GDB server.
#----------------------------------------------------------------------
#
# Difference from original code
#
# This script is based on armv7_cortex_m_target_definition.py
#  * Ref: https://github.com/llvm/llvm-project/blob/master/lldb/examples/python/armv7_cortex_m_target_defintion.py
# I rewrite this code to support aarch64.
#----------------------------------------------------------------------

from lldb import *

# DWARF register numbers
name_to_gdb_regnum = {
    'x0'   : 0 , 
    'x1'   : 1 ,
    'x2'   : 2 ,
    'x3'   : 3 ,
    'x4'   : 4 ,
    'x5'   : 5 ,
    'x6'   : 6 ,
    'x7'   : 7 ,
    'x8'   : 8 ,
    'x9'   : 9 ,
    'x10'  : 10,
    'x11'  : 11,
    'x12'  : 12,
    'x13'  : 13,
    'x14'  : 14,
    'x15'  : 15,
    'x16'  : 16,
    'x17'  : 17,
    'x18'  : 18,
    'x19'  : 19,
    'x20'  : 20,
    'x21'  : 21,
    'x22'  : 22,
    'x23'  : 23,
    'x24'  : 24,
    'x25'  : 25,
    'x26'  : 26,
    'x27'  : 27,
    'x28'  : 28,
    'x29'  : 29,
    'x30'  : 30,
    'sp'   : 31,
    'pc'   : 32,
    'cpsr' : 33,
}

name_to_generic_regnum = {
    'pc' : LLDB_REGNUM_GENERIC_PC,
    'sp' : LLDB_REGNUM_GENERIC_SP,
    'x29' : LLDB_REGNUM_GENERIC_FP,
    'x30' : LLDB_REGNUM_GENERIC_RA,
    'x0' : LLDB_REGNUM_GENERIC_ARG1,
    'x1' : LLDB_REGNUM_GENERIC_ARG2,
    'x2' : LLDB_REGNUM_GENERIC_ARG3,
    'x3' : LLDB_REGNUM_GENERIC_ARG4,
    'x4' : LLDB_REGNUM_GENERIC_ARG5,
    'x5' : LLDB_REGNUM_GENERIC_ARG6,
    'x6' : LLDB_REGNUM_GENERIC_ARG7,
    'x7' : LLDB_REGNUM_GENERIC_ARG8,
}


def get_reg_num (reg_num_dict, reg_name):
    if reg_name in reg_num_dict:
        return reg_num_dict[reg_name]
    return LLDB_INVALID_REGNUM
    
armv8_register_infos = [
{ 'name':'x0'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg1' },
{ 'name':'x1'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg2' },
{ 'name':'x2'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg3' },
{ 'name':'x3'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg4' },
{ 'name':'x4'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg5' },
{ 'name':'x5'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg6' },
{ 'name':'x6'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg7' },
{ 'name':'x7'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'arg8' },
{ 'name':'x8'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x9'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x10'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x11'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x12'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x13'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x14'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x15'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x16'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x17'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x18'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x19'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x20'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x21'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x22'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x23'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x24'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x25'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x26'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x27'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x28'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'x29'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'fp'},
{ 'name':'x30'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo, 'alt-name':'lr'},
{ 'name':'sp'  , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'pc'   , 'set':0, 'bitsize':64 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
{ 'name':'cpsr' , 'set':0, 'bitsize':32 , 'encoding':eEncodingUint  , 'format':eFormatAddressInfo },
]

g_target_definition = None

def get_target_definition ():
    global g_target_definition
    if g_target_definition == None:
        g_target_definition = {}
        offset = 0
        for reg_info in armv8_register_infos:
            reg_name = reg_info['name']

            if 'slice' not in reg_info and 'composite' not in reg_info:
                reg_info['offset'] = offset
                offset += reg_info['bitsize'] // 8

            # Set the generic register number for this register if it has one    
            reg_num = get_reg_num(name_to_generic_regnum, reg_name)
            if reg_num != LLDB_INVALID_REGNUM:
                reg_info['generic'] = reg_num

            # Set the GDB register number for this register if it has one
            reg_num = get_reg_num(name_to_gdb_regnum, reg_name)
            if reg_num != LLDB_INVALID_REGNUM:
                reg_info['gdb'] = reg_num

        g_target_definition['sets'] = ['General Purpose Registers']
        g_target_definition['registers'] = armv8_register_infos
        g_target_definition['host-info'] = { 'triple'   : 'aarch64--', 'endian': eByteOrderLittle }
        g_target_definition['g-packet-size'] = offset
    return g_target_definition

def get_dynamic_setting(target, setting_name):
    if setting_name == 'gdb-server-target-definition':
        return get_target_definition()
