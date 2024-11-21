#!/usr/bin/env python
# encoding: utf-8

# Modifications copyright Amazon.com, Inc. or its affiliates.

# Carlos Rafael Giani, 2006 (dv)
# Tamas Pal, 2007 (folti)
# Nicolas Mercier, 2009
# Matt Clarkson, 2012

# System Imports
import os
import sys
import tempfile

winreg_available = True
try:
    import winreg
except ImportError:
    winreg_available = False
    pass

# waflib imports
from waflib import Utils, Task, Logs, Options, Errors
from waflib.Logs import debug, warn
from waflib.Tools import c_preproc, ccroot, c, cxx, ar
from waflib.Configure import conf
from waflib.TaskGen import feature, after, after_method, before_method
import waflib.Node

# The compiler will issue a warning if some flags are specified more than once.
# The command is constructed from subsets that may have conflicting flags
# This list of lists contains all the set of flags that are made unique
UNIQUE_FLAGS_LIST = [
    ["/arch:IA32", "/arch:SSE", "/arch:SSE2", "/arch:AVX", "/arch:AVX2"],   # code gen for arch
    ["/clr", "/clr:initialAppDomain", "/clr:noAssembly", "/clr:nostdlib", "/clr:pure", "/clr:safe"],    # common language runtime
    ["/EHs", "/EHa", "/EHac", "/EHsc"],                                     # exception handling
    ["/errorReport:none", "/errorReport:prompt", "/errorReport:queue", "/errorReport:send"],        # report internal compiler errors
    ["/favor:blend", "/favor:ATOM", "/favor:AMD64", "/favor:INTEL64"],      # optimize for arch
    ["/fp:precise", "/fp:except", "/fp:except-", "/fp:fast", "/fp:strict"], # floating point behavior
    ["/Gd", "/Gr", "/Gv", "/Gz"],                                           # calling convention
    ["/GL", "/GL-"],                                    # whole program optimization
    ["/GR", "/GR-"],                                    # runtime type information
    ["/GS", "/GS-"],                                    # buffer security checks
    ["/Gs", "/Gs0", "/Gs4096"],                         # control stack checking calls
    ["/Gw", "/Gw-"],                                    # global data optimization
    ["/Gy", "/Gy-"],                                    # enable function level linking
    ["/O1", "/O2", "/Od", "/Ox"],                       # optimization level
    ["/Ob0", "/Ob1", "/Ob2"],                           # inline expansion
    ["/Oi", "/Oi-"],                                    # intrinsics
    ["/Os", "/Ot"],                                     # favor small code/ fast code
    ["/Oy", "/Oy-"],                                    # frame pointer omission
    ["/MD", "/MT", "/LD", "/MDd", "/MTd", "/LDd"],      # runtime library
    ["/RTC1","/RTCc","/RTCs","/RTCu"],                  # runtime error checks
    ["/volatile","/volatile:iso", "/volatile:ms"],      # volatile keyword handling
    ["/vd0", "/vd1", "/vd2"],                           # disable construction displacements
    ["/ZW", "/ZW:nostdlib"],                            # windows runtime compilation
    ["/sdl", "/sdl-"],                                  # enable additional security checks
    ["/vmb", "/vmg"],                                   # always declare a class before using a pointer to it
    ["/vmm", "/vms", "/vmv"],                           # inheritance of yet-to-be-defined classes
    ["/W0", "/W1", "/W2", "/W3", "/W4"],                # error level
    ["/WX", "/WX-", "/WX:NO"],                          # treat warnings as errors
    ["/Z7", "/Zi", "/ZI"],                              # debug information format
    ["/Za", "/Ze"],                                     # disable language extensions
    ["/Zc:forScope", "/Zc:forScope-"],                  # for loop scope conformance
    ["/Zc:wchar_t", "/Zc:wchar_t-"],                    # wchar_t maps to __wchar_t
    ["/Zc:auto", "/Zc:auto-"],                          # deduce variable type
    ["/Zc:trigraphs", "/Zc:trigraphs-"],                # character substitutions if character isn't in charpage
    ["/Zc:rvalueCast", "/Zc:rvalueCast-"],              # enforce type conversion rules
    ["/Zc:strictStrings", "/Zc:strictStrings-"],        # disable string literal type conversion
    ["/Zc:inline", "/Zc:inline-"],                      # remove unreferenced comdat sections
    ["/Zp", "/Zp:1", "/Zp:2", "/Zp:4", "/Zp:8", "/Zp:16"]   # struct member alignment
]

# convert list of flags that must be unique to dictionary
UNIQUE_FLAGS_DICT = {}
for idx, flags in enumerate(UNIQUE_FLAGS_LIST):
    assert(isinstance(flags,list))
    for flag in flags:
        UNIQUE_FLAGS_DICT[flag] = idx     # all flags from the list have the same value, just using index as a dummy unique val


def exec_mf(self):
        """
        Create the manifest file
        """        
        env = self.env
        mtool = env['MT']
        if not mtool:
                return 0

        self.do_manifest = False

        outfile = self.outputs[0].abspath()

        manifest = None
        for out_node in self.outputs:
                if out_node.name.endswith('.manifest'):
                        manifest = out_node.abspath()
                        break

        #Disabling manifest for incremental link
        if '/INCREMENTAL' in env['LINKFLAGS']:
            manifest = None

        if manifest is None:
                # Should never get here.  If we do, it means the manifest file was
                # never added to the outputs list, thus we don't have a manifest file
                # to embed, so we just return.
                return 0

        # embedding mode. Different for EXE's and DLL's.
        # see: http://msdn2.microsoft.com/en-us/library/ms235591(VS.80).aspx
        mode = ''
        if 'cprogram' in self.generator.features or 'cxxprogram' in self.generator.features:
                mode = '1'
        elif 'cshlib' in self.generator.features or 'cxxshlib' in self.generator.features:
                mode = '2'

        debug('msvc: embedding manifest in mode %r' % mode)

        lst = []
        lst.append(env['MT'])
        lst.extend(Utils.to_list(env['MTFLAGS']))
        lst.extend(['-manifest',manifest])
        
        if hasattr(self.generator, 'additional_manifests'):
            if not isinstance(self.generator.additional_manifests, list): # the additional manifests could be a string
                self.generator.additional_manifests = [self.generator.additional_manifests]
            for element in self.generator.additional_manifests: # add each one with its own path
                 lst.append( self.generator.path.abspath() + '/' + element)
        lst.append('-outputresource:%s;%s' % (outfile, mode))

        # note that because we call exec_command and give it a list of params, these become the subprocess argv*
        # and thus it is not necessary for us to escape them with quotes or anything like that.
        lst = [lst]        
        return self.exec_command(*lst)
                
def quote_response_command(self, flag):
        flag = flag.replace('\\', '\\\\') # escape any backslashes
        flag = flag.replace('"', '\\"') # escape any quotes
        if flag.find(' ') > -1:
                for x in ('/LIBPATH:', '/IMPLIB:', '/OUT:', '/I'):
                        if flag.startswith(x):
                                flag = '%s"%s"' % (x, flag[len(x):])
                                break
                else:
                        flag = '"%s"' % flag
        return flag

def exec_response_command(self, cmd, **kw):
    # not public yet
    try:
        tmp = None
        if sys.platform.startswith('win') and isinstance(cmd, list) and len(' '.join(cmd)) >= 16384:
                tmp_files_folder = self.generator.bld.get_bintemp_folder_node().make_node('TempFiles')
                program = cmd[0] #unquoted program name, otherwise exec_command will fail
                cmd = [self.quote_response_command(x) for x in cmd]

                # Determine an appropriate filename for the output file (displayed by Incredibuild)
                if self.outputs and len(self.outputs[0].abspath()):
                    tmp_file_name = os.path.basename(self.outputs[0].abspath())
                else:
                    # strips quotes off the FRONT in case its a string like '"something="somethingelse"' which would cause issues
                    out_file = os.path.split(cmd[-1].strip('"'))
                    tmp_file_name = out_file[1]
                (fd, tmp) = tempfile.mkstemp(prefix=tmp_file_name, dir=tmp_files_folder.abspath())
                os.write(fd, '\r\n'.join(cmd[1:]).encode())
                os.close(fd)
                cmd = [program, '@' + tmp]
        # no return here, that's on purpose
        ret = self.generator.bld.exec_command(cmd, **kw)

    finally:
        if tmp:
            try:
                os.remove(tmp)
            except OSError:
                pass # anti-virus and indexers can keep the files open -_-
    return ret

########## stupid evil command modification: concatenate the tokens /Fx, /doc, and /x: with the next token
def exec_command_msvc(self, *k, **kw):
    """
    Change the command-line execution for msvc programs.
    Instead of quoting all the paths and keep using the shell, we can just join the options msvc is interested in
    """
    # If bullseye coverage tool is in the environment, and we are executing the CXX or C compiler, then
    # we prefix with the bullseye coverage tool. Otherwise, just run the regular tools.
    # Ideally we need a way to do this cleanly on other platforms, but this implies a post hook of some kind to change
    # the CXX task run_str, and I could not immediately see a clean way to do that, especially conditionally as
    # we need to do below.
    if 'BULL_COVC' in self.env:
        
        excluded_modules = getattr(self.generator.bld.options,'bullseye_excluded_modules',"").replace(' ', '').split(',')
        if "" in excluded_modules:
            excluded_modules.remove("")

        # Figure out which package we are building, and check if it is in the list of packages we want coverage for
        # If the package list is empty, then we do coverage building for the whole project.
        # This applies to the CC/CXX steps. We must always link with coverage if we are going to get measurements.
        included_modules = getattr(self.generator.bld.options,'bullseye_included_modules',"").replace(' ', '').split(',')
        if "" in included_modules:
            included_modules.remove("")

        if self.generator.name not in excluded_modules and (not included_modules or self.generator.name in included_modules):
            if k[0][0] == self.env['CXX'] or k[0][0] == self.env['CC']:
                k = ([self.env['BULL_COVC'], '--file', self.env['BULL_COV_FILE']] + k[0],)
                # Bullseye has problems with external include directories, so strip out the external flag if it appears. 
                # Disable warnings as errors since we are not using external include directories, and so 3rd party will generate warnings.
                k = tuple([[element.replace("/external:I", "/I") for element in k[0] if element != '/WX']]) + k[1:]

    # We must link with bullseye regardless of which way the project is set (with or without coverage) to avoid link errors with included libraries.
    if 'BULL_COVLINK' in self.env and (k[0][0] == self.env['LINK'] or k[0][0] == self.env['LINK_CXX'] or k[0][0] == self.env['LINK_CC']):
        k = ([self.env['BULL_COVLINK']] + k[0],)

    # 1) Join options that carry no space are joined e.g. /Fo FilePath -> /FoFilePath
    # 2) Join options that carry a ':' as last character : e.g. /OUT: FilePath -> /OUT:FilePath
    if isinstance(k[0], list):
        lst = []
        carry = ''
        join_with_next_list_item = ['/Fo', '/doc', '/Fi', '/Fa']
        for a in k[0]:
            if a in  join_with_next_list_item or a[-1] == ':':
                carry = a
            else:
                lst.append(carry + a)
                carry = ''

        k = [lst]

    bld = self.generator.bld
    try:
        if not kw.get('cwd', None):
            kw['cwd'] = bld.cwd
    except AttributeError:
        bld.cwd = kw['cwd'] = bld.variant_dir

    ret = self.exec_response_command(k[0], **kw)
    
    if not ret and getattr(self, 'do_manifest', None):
        ret = self.exec_mf()
    return ret
        
def wrap_class(class_name):
        """
        Manifest file processing and @response file workaround for command-line length limits on Windows systems
        The indicated task class is replaced by a subclass to prevent conflicts in case the class is wrapped more than once
        """    
        cls = Task.classes.get(class_name, None)
        if not cls:
                return

        derived_class = type(class_name, (cls,), {})
        def exec_command(self, *k, **kw):
                if self.env['CC_NAME'] == 'msvc':
                        return self.exec_command_msvc(*k, **kw)
                else:
                        return super(derived_class, self).exec_command(*k, **kw)

        # Chain-up monkeypatch needed since exec_command() is in base class API
        derived_class.exec_command = exec_command

        # No chain-up behavior needed since the following methods aren't in
        # base class API
        derived_class.exec_response_command = exec_response_command
        derived_class.quote_response_command = quote_response_command
        derived_class.exec_command_msvc = exec_command_msvc
        derived_class.exec_mf = exec_mf

        return derived_class

for k in 'c cxx cprogram cxxprogram cshlib cxxshlib cstlib cxxstlib'.split():
    wrap_class(k)


@feature('cxxprogram', 'cxxshlib', 'cprogram', 'cshlib', 'cxx', 'c')
@after_method('apply_incpaths')
@after_method('add_pch_to_dependencies')
def set_pdb_flags(self):            
    if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        return  

    if not self.bld.is_option_true('generate_debug_info'):
        return

    # find the last debug symbol type of [/Z7, /Zi, /ZI] applied in cxxflags.
    last_debug_option = ''
    for opt in reversed(self.env['CXXFLAGS']):
        if opt in ['/Z7', '/Zi', '/ZI']:
            last_debug_option = opt
            break

    if last_debug_option in ['/Zi', '/ZI']:
        # Compute PDB file path
        pdb_folder = self.path.get_bld().make_node(str(self.target_uid))
        pdb_cxxflag = '/Fd{}'.format(pdb_folder.abspath())


        # Make sure the PDB folder exists
        pdb_folder.mkdir()
    
        # Add CXX and C Flags
        for t in getattr(self, 'compiled_tasks', []):
            t.env.append_value('CXXFLAGS', pdb_cxxflag)
            t.env.append_value('CFLAGS', pdb_cxxflag)

        # Add PDB also to Precompiled header.  pch_task is not in compiled_tasks
        if getattr(self, 'pch_task', None):
            self.pch_task.env.append_value('CXXFLAGS', pdb_cxxflag)
            self.pch_task.env.append_value('CFLAGS', pdb_cxxflag)


def is_node_qt_rc_generated(self,node):

    if node.is_child_of(self.bld.bldnode):
        raw_name = os.path.splitext(os.path.basename(node.abspath()))[0]
        if raw_name.endswith('_rc'):
            return True
    return False

@feature('cxx')
@before_method('process_source')
def add_pch_msvc(self):
    if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        return

    if Utils.unversioned_sys_platform() != 'win32':
        return

    # Create Task to compile PCH
    if not getattr(self, 'pch', ''):
        return

    if not self.bld.is_option_true('use_precompiled_header'):
        return

    # Always assume only one PCH File
    pch_source = self.to_nodes(self.pch)[0]

    self.pch_header = pch_source.change_ext('.h')
    self.pch_header_name = os.path.split(self.pch_header.abspath())[1]

    # Generate PCH per target project idx
    # Avoids the case where two project have the same PCH output path but compile the PCH with different compiler options i.e. defines, includes, ...
    # creating the output node
    objfolder = self.bld.bldnode.find_or_declare("obj")
    m = hashlib.md5()
    m.update(pch_source.parent.abspath())
    md5folder = objfolder.find_or_declare(m.hexdigest())

    self.pch_file = md5folder.find_or_declare(pch_source.change_ext_name('.%d.pch' % self.target_uid))
    self.pch_object = md5folder.find_or_declare(pch_source.change_ext_name('.%d.obj' % self.target_uid))
    
    # Create PCH Task
    self.pch_task = pch_task = self.create_task('pch_msvc', pch_source, [self.pch_object, self.pch_file])
    pch_task.env.append_value('PCH_NAME', self.pch_header_name)
    pch_task.env.append_value('PCH_FILE', '/Fp' + self.pch_file.abspath())
    pch_task.env.append_value('PCH_OBJ', self.pch_object.abspath())
    # Pre-compiled headers retains pragma(warning) during compilation of the actual pch file
    # therefore any warnings that occur during that compilation is unable to be suppressed.
    # In this case warning C4251 is suppressed for MSVC compilers which is a warning that 
    # class that performs a dllexport needs to have it's data members to have a dll-interface as well
    # https://docs.microsoft.com/en-us/cpp/build/creating-precompiled-header-files?view=vs-2019#pragma-consistency
    # The warning can still trigger in code that uses the precompiled header, just not in the compiled file itself
    if not pch_task.env['CXXFLAGS'] or not isinstance(pch_task.env['CXXFLAGS'], list):
        pch_task.env['CXXFLAGS'] = ['/wd4251']
    else:
        pch_task.env['CXXFLAGS'].append('/wd4251')

@feature('cxx')
@after_method('apply_incpaths')
def add_pch_to_dependencies(self):
    if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        return

    # Create Task to compile PCH
    if not getattr(self, 'pch_object', ''):
        return

    pch_abs_path = self.pch_file.abspath()
    pch_flag = '/Fp' + pch_abs_path
    pch_header = '/Yu' + self.pch_header_name

    # Append PCH File to each compile task
    for t in getattr(self, 'compiled_tasks', []):   
        input_file = t.inputs[0].abspath()
        file_specific_settings = self.file_specifc_settings.get(input_file, None)
        if file_specific_settings and 'disable_pch' in file_specific_settings and file_specific_settings['disable_pch'] == True:
            continue # Don't append PCH to files for which we don't use them
            
        if getattr(t, 'disable_pch', False) == True:
            continue # Don't append PCH to files for which we don't use them
            
        if t.__class__.__name__ in ['cxx','qxx']: #Is there a better way to ensure cpp only?

            if is_node_qt_rc_generated(self,t.inputs[0]):
                t.env.append_value('CXXFLAGS', '/Y-')
            else:
                t.env.append_value('CXXFLAGS', pch_header)
                t.env.append_value('CXXFLAGS', pch_flag)

                # Append PCH to task input to ensure correct ordering
                t.dep_nodes.append(self.pch_object)

    # Append the pch object to the link task
    if getattr(self, 'link_task', None):
        self.link_task.inputs.append(self.pch_object)


class pch_msvc(waflib.Task.Task):
    run_str = '${CXX} ${PCH_CREATE_ST:PCH_NAME} ${CXXFLAGS} ${CPPPATH_ST:INCPATHS} ${SYSTEM_CPPPATH_ST:SYSTEM_INCPATHS} ${DEFINES_ST:DEFINES} ${SRC} ${CXX_TGT_F}${PCH_OBJ} ${PCH_FILE}'
    scan    = c_preproc.scan
    color   = 'BLUE'
    
    def exec_command(self, *k, **kw):   
        return exec_command_msvc(self, *k, **kw)
    
    def exec_response_command(self, *k, **kw):  
        return exec_response_command(self, *k, **kw)    

    def quote_response_command(self, *k, **kw): 
        return quote_response_command(self, *k, **kw)

    def exec_mf(self, *k, **kw):    
        return exec_mf(self, *k, **kw)


def strip_all_but_last_dependent_options(flags):
    seen = set()
    delete = []
    for idx, flag in enumerate(reversed(flags)):
        try:
            val = UNIQUE_FLAGS_DICT[flag]
            if val not in seen:
                seen.add(val)
                continue
            # mark for delete
            delete.append(len(flags) -1 -idx)
        except:
            pass
    for idx in delete:
        del flags[idx]


def verify_options_common(env):
    strip_all_but_last_dependent_options(env.CFLAGS)
    strip_all_but_last_dependent_options(env.CXXFLAGS)


@feature('c', 'cxx')
@after_method('apply_link')
@after_method('add_pch_to_dependencies')
def verify_compiler_options_msvc(self):
    if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        return
    
    if Utils.unversioned_sys_platform() != 'win32':
        return
        
    # Verify compiler option (strip all but last for dependant options)
    for t in getattr(self, 'compiled_tasks', []):           
        verify_options_common(t.env)

    # Verify pch_task options (strip all but last for dependant options)
    if hasattr(self, 'pch_task'):
        verify_options_common(self.pch_task.env)

    # Strip unsupported ARCH linker option
    if hasattr(self, 'link_task'):
        del self.link_task.env['ARCH']
    
    
#############################################################################
# Code for auto-recognition of Visual Studio Compiler and Windows SDK Path
# Taken from the original WAF code
#############################################################################

def _is_valid_win_sdk(path, is_universal_versioning, desired_version=''):
    # Successfully installed windows kits have rc.exe. This file is a downstream dependency of vcvarsall.bat.
    def _check_for_rc_file(path):
        rc_x64 = os.path.join(path, 'x64\\rc.exe')
        rc_x86 = os.path.join(path, 'x86\\rc.exe')
        return os.path.isfile(rc_x64) or os.path.isfile(rc_x86)

    bin_dir = os.path.join(path, 'bin')
    include_dir = os.path.join(path, 'include')
    if is_universal_versioning:
        potential_sdks = [desired_version] if desired_version else []
        if os.path.isdir(include_dir):
            # lexically sort the 10.xxx versions in reverse so that latest/highest is first
            potential_sdks += sorted(os.listdir(include_dir), reverse=True)
        sdk10_versions = [entry for entry in potential_sdks if entry.startswith('10.')]
        for sub_version in sdk10_versions:
            sub_version_folder = os.path.join(include_dir, sub_version)
            if os.path.isdir(os.path.join(sub_version_folder, 'um')):
                # check for rc.exe in the sub_version folder's bin, or in the root 10 bin, we just need at least one
                for bin_path in (os.path.join(os.path.join(path, 'bin'), sub_version), bin_dir):
                    if _check_for_rc_file(bin_path):
                        return True, sub_version, bin_path
    else:
        if _check_for_rc_file(bin_dir):
            version = path.split('\\')[-2]
            return True, version, bin_dir

    return False, '', ''

def _find_win_sdk_root(winsdk_hint):
    """
        Use winreg to find a valid installed windows kit.
        Returns empty string if no valid version was found.

        See visual studio compatibility charts here:
        https://www.visualstudio.com/en-us/productinfo/vs2015-compatibility-vs

        """
    try:
        installed_roots = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\Wow6432node\\Microsoft\\Windows Kits\\Installed Roots')
    except WindowsError:
        try:
            installed_roots = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots')
        except WindowsError:
            return ''

    if winsdk_hint.startswith('10'):
        try:
            path, type = Utils.winreg.QueryValueEx(installed_roots, 'KitsRoot10')
            return path
        except WindowsError:
            pass

    elif winsdk_hint.startswith('8'):
        try:
            path, type = Utils.winreg.QueryValueEx(installed_roots, 'KitsRoot81')
            return path
        except WindowsError:
            pass

    return ''


def find_valid_wsdk_version():
    path = _find_win_sdk_root("10")
    if path:
        is_valid, version, bin_path = _is_valid_win_sdk(path, True)
        if is_valid:
            return version

    # No root for sdk 10 found, try 8.1
    path = _find_win_sdk_root("8.1")
    if path:
        is_valid, version, bin_path = _is_valid_win_sdk(path, False)
        if is_valid:
            return version

    return ''

cached_folders = {}

@conf
def detect_visual_studio_vc_path(ctx, version):
    """
    Attempt to locate the installed visual studio VC path
    :param version: Visual studio version (12.0, 14.0, etc)
    :param fallback_path: In case the registry key cannot be found, fall back and see if this path exists
    :return: The path to use for the visual studio VC folder
    """
    if not winreg_available:
        raise SystemError('[ERR] Windows registry is not supported on this platform.')

    cache_key = 'detect_visual_studio_vc_path_{}'.format(version)
    if cache_key in cached_folders:
        return cached_folders[cache_key]

    vs_tools_path = os.path.normpath('C:\\Program Files (x86)\\Microsoft Visual Studio ' + version + '\\VC')
    if not os.path.isdir(vs_tools_path):
        vs_tools_path = ''

    try:
        vs_regkey = 'Software\\Microsoft\\VisualStudio\\{}_Config\\Setup\\vs'.format(version)
        vs_tools_reg_key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, vs_regkey, 0, winreg.KEY_READ)
        (vs_tools_path, reg_type) = winreg.QueryValueEx(vs_tools_reg_key, 'ProductDir')
        vs_tools_path = vs_tools_path.encode('ascii')  # Make ascii string (as we get unicode)
        vs_tools_path += 'VC'
    except:
        Logs.warn('[WARN] Unable to find visual studio tools path from the registry.')

    if vs_tools_path == '':
        raise SystemError('[ERR] Unable to locate the visual studio VC folder for (vs version {})'.format(version))

    if not os.path.isdir(vs_tools_path):
        raise SystemError('[ERR] Unable to locate the visual studio VC folder {} for (vs version {})'.format(vs_tools_path, version))

    cached_folders[cache_key] = vs_tools_path
    return vs_tools_path

@conf
def detect_windows_kits_include_path(ctx, version_if_available_otherwise_latest):
    """
    Attempt to locate the windows sdk include path
    :param
    :return:
    """

    if not winreg_available:
        raise SystemError('[ERR] Windows registry is not supported on this platform.')

    cache_key = 'windows_sdk_include_path'
    if cache_key in cached_folders:
        return cached_folders[cache_key]

    requested_version = version_if_available_otherwise_latest

    #best guess first
    normal_root = 'C:\\Program Files (x86)\\Windows Kits\\'
    windows_sdk_include_path = normal_root + requested_version + '\\Include'
    if not os.path.isdir(windows_sdk_include_path):
        for ver in ['10', '8.1', 'last']:
            if ver == 'last':
                windows_sdk_include_path = ''
            else:
                windows_sdk_include_path = normal_root + ver + '\\Include'
                if os.path.isdir(windows_sdk_include_path):
                    break
    try:
        windows_sdk_installed_roots_key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\Wow6432node\\Microsoft\\Windows Kits\\Installed Roots', winreg.KEY_READ)
    except WindowsError:
        try:
            windows_sdk_installed_roots_key = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots')
        except WindowsError:
            if windows_sdk_include_path == '':
                raise SystemError('[ERR] Unable to locate the Windows SDK include folder')
            Logs.warn('[WARN] Unable to find windows kits include folder keys from the registry. Falling back to path {}'.format(windows_sdk_include_path))
            return windows_sdk_include_path
    try:
        #enumerate all keys below this key, they correspond to the installed versions
        exact_version = None
        versions = []
        index = 0
        while not exact_version:
            try:
                value = winreg.EnumValue(windows_sdk_installed_roots_key, index)
                if 'KitsRoot' in value[0]:
                    if value[0] == 'KitsRoot81':
                        versions.append((8.1, value[1]))
                        if requested_version == '8.1':
                            exact_version = value[1]
                    if value[0] == 'KitsRoot10':
                        versions.append((10.0, value[1]))
                        if requested_version == '10.0':
                            exact_version = value[1]
            except EnvironmentError:
                break
            else:
                index += 1
    except EnvironmentError:
        pass

    if not versions:
        if windows_sdk_include_path == '':
            raise SystemError('[ERR] Unable to locate the Windows SDK include folder')
        Logs.warn('[WARN] Unable to find windows kits include folder keys from the registry. Falling back to path {}'.format(windows_sdk_include_path))
        return windows_sdk_include_path

    if exact_version:
        windows_sdk_include_path = exact_version
    else:
        #sort them reversed, so the latest should be first
        versions.sort(reverse=True)
        windows_sdk_include_path = versions[0][1]
    windows_sdk_include_path += 'Include'

    if windows_sdk_include_path == '':
        raise SystemError('[ERR] Unable to locate the Windows SDK include folder')
    if not os.path.exists(windows_sdk_include_path):
        raise SystemError('[ERR] Unable to locate the Windows SDK include folder {}'.format(windows_sdk_include_path))

    cached_folders[cache_key] = windows_sdk_include_path
    return windows_sdk_include_path