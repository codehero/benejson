# Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
# See the file LICENSE for full license information.

#Default construction environment

import os

mydir = os.getcwd()
#Set default C++ building flags for both libraries and executables
default_env = Environment(ENV = os.environ)
default_env.Append(CPPPATH = [mydir + '/include'])
default_env.Append(CFLAGS = ' -Wall')
default_env.Append(CFLAGS = ' -std=c99')
#default_env.Append(CCFLAGS = ' -O2 -fomit-frame-pointer')
default_env.Append(CCFLAGS = ' -O0 -g')

if 'CROSS' in os.environ:
	cross = os.environ['CROSS']
	default_env.Append(CROSS = cross)
	default_env.Replace(CC = cross + 'gcc')
	default_env.Replace(CXX = cross + 'g++')
	default_env.Replace(LD = cross + 'ld')


default_env.LibDest = mydir + '/lib'
default_env.BinDest = mydir + '/bin'
default_env.IncDest = mydir + '/include'

#Set linking flags for executables
bin_env = default_env.Clone()
bin_env.Append(LIBPATH = [bin_env.LibDest])

#set linking flags for libraries
lib_env = default_env.Clone()
lib_env.Append(LINKFLAGS = ['-fPIC'])

SConscript('benejson/SConscript', exports='bin_env lib_env', variant_dir='build/benejson', duplicate = 0)
SConscript('tests/SConscript', exports='bin_env lib_env', variant_dir='build/tests', duplicate = 0)
