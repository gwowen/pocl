/* pocl_llvm_api.cc: C wrappers for calling the LLVM/Clang C++ APIs

   Copyright (c) 2013 Kalle Raiskila and
                      Pekka Jääskeläinen
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "config.h"

#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Linker.h"
#include "llvm/PassManager.h"
#include "llvm/Bitcode/ReaderWriter.h"

#ifdef LLVM_3_2
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Support/IRReader.h"
#include "llvm/DataLayout.h"
#else
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IRReader/IRReader.h"
#endif

#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sys/stat.h>

#include <iostream>
#include <vector>
#include <sstream>
#include <string>

// Note - LLVM/Clang uses symbols defined in Khronos' headers in macros, 
// causing compilation error if they are included before the LLVM headers.
#include "pocl_llvm.h"
#include "install-paths.h"
#include "LLVMUtils.h"

using namespace clang;
using namespace llvm;

#if defined LLVM_3_2 || defined LLVM_3_3
#include "llvm/Support/raw_ostream.h"
#define F_Binary llvm::raw_fd_ostream::F_Binary
#else
using llvm::sys::fs::F_Binary;
#endif

//#define DEBUG_POCL_LLVM_API

//#define INCLUDE_UNFINISHED

/* "emulate" the pocl_build script.
 * This compiles an .cl file into LLVM IR 
 * (the "program.bc") file.
 * unlike the script, a intermediate preprocessed 
 * program.bc.i file is not produced.
 */
int call_pocl_build(cl_device_id device, 
                    const char* source_file_name,
                    const char* binary_file_name,
                    const char* device_tmpdir,
                    const char* user_options)

{ 

  // Use CompilerInvocation::CreateFromArgs to initialize
  // CompilerInvocation. This way we can reuse the Clang's
  // command line parsing.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagID = 
    new clang::DiagnosticIDs();
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpts = 
    new clang::DiagnosticOptions();
  clang::TextDiagnosticBuffer *diagsBuffer = 
    new clang::TextDiagnosticBuffer();

  clang::DiagnosticsEngine diags(diagID, &*diagOpts, diagsBuffer);

  CompilerInstance CI;
  CompilerInvocation &pocl_build = CI.getInvocation();

  // TODO: call device->prepare_build() that can return
  // device-specific switches too, to replace the build_program()
  // API TCE uses to include the custom op macros.
  std::stringstream ss;

  if (device->init_build != NULL) 
    {
      assert (device_tmpdir != NULL);
      const char *device_switches = 
        device->init_build (device->data, device_tmpdir);
      if (device_switches != NULL) 
        {
          ss << device_switches << " ";
        }
      free ((void*)device_switches);
    }

  // Ensure no optimizations are done to not mess up the
  // barrier semantics etc.
  ss << "-O0 ";

  // Remove the inline keywords to force the user functions
  // to be included in the program. Otherwise they will
  // be removed and not inlined due to -O0.
  ss << "-Dinline= ";
  // The current directory is a standard search path.
  ss << "-I. ";

  ss << "-fno-builtin ";

  // This is required otherwise the initialization fails with
  // unknown triplet ''
  ss << "-triple=" << device->llvm_target_triplet << " ";
  if (device->llvm_cpu != NULL)
    ss << "-target-cpu " << device->llvm_cpu << " ";
  ss << user_options << " ";
  std::istream_iterator<std::string> begin(ss);
  std::istream_iterator<std::string> end;
  std::istream_iterator<std::string> i = begin;
  std::vector<const char*> itemcstrs;
  std::vector<std::string> itemstrs;
  while (i != end) 
    {
      itemstrs.push_back(*i);
      itemcstrs.push_back(itemstrs.back().c_str());
      ++i;
    }
#ifdef DEBUG_POCL_LLVM_API
  // TODO: for some reason the user_options are replicated,
  // they appear twice in a row in the output
  std::cerr << "### options: " << ss.str() 
            << "user_options: " << user_options << std::endl;
#endif

  if (!CompilerInvocation::CreateFromArgs
      (pocl_build, itemcstrs.data(), itemcstrs.data() + itemcstrs.size(), 
       diags)) 
    {
      for (TextDiagnosticBuffer::const_iterator i = diagsBuffer->err_begin(), 
             e = diagsBuffer->err_end(); i != e; ++i) 
        {
          // TODO: transfer the errors to clGetProgramBuildInfo
          std::cerr << "error: " << (*i).second << std::endl;
        }
      for (TextDiagnosticBuffer::const_iterator i = diagsBuffer->warn_begin(), 
             e = diagsBuffer->warn_end(); i != e; ++i) 
        {
          // TODO: transfer the warnings to clGetProgramBuildInfo
          std::cerr << "warning: " << (*i).second << std::endl;
        }
      return CL_INVALID_BUILD_OPTIONS;
    }
  
  LangOptions *la = pocl_build.getLangOpts();
  pocl_build.setLangDefaults
    (*la, clang::IK_OpenCL, clang::LangStandard::lang_opencl12);
  
  // the per-file types don't seem to override this 
  la->OpenCLVersion = 120;
  la->FakeAddressSpaceMap = true;
  la->Blocks = true; //-fblocks
  la->MathErrno = false; // -fno-math-errno
  la->NoBuiltin = true;  // -fno-builtin
#ifndef LLVM_3_2
  la->AsmBlocks = true;  // -fasm (?)
#endif

  // -Wno-format
  PreprocessorOptions &po = pocl_build.getPreprocessorOpts();
  po.addMacroDef("__OPENCL_VERSION__=120"); // -D__OPENCL_VERSION_=120

  std::string kernelh;
  if (getenv("POCL_BUILDING") != NULL)
    { 
      kernelh  = SRCDIR;
      kernelh += "/include/_kernel.h";
    }
  else
    {
      kernelh = PKGDATADIR;
      kernelh += "/include/_kernel.h";
    }
  po.Includes.push_back(kernelh);

  // TODO: user_options (clBuildProgram options) are not passed

  clang::TargetOptions &ta = pocl_build.getTargetOpts();
  ta.Triple = device->llvm_target_triplet;
  if (device->llvm_cpu != NULL)
    ta.CPU = device->llvm_cpu;

  // printf("### Triple: %s, CPU: %s\n", ta.Triple.c_str(), ta.CPU.c_str());

  // FIXME: print out any diagnostics to stdout for now. These should go to a buffer for the user
  // to dig out. (and probably to stdout too, overridable with environment variables) 
#ifdef LLVM_3_2
  CI.createDiagnostics(0, NULL);
#else
  CI.createDiagnostics();
#endif 
 
  FrontendOptions &fe = pocl_build.getFrontendOpts();
  // The CreateFromArgs created an stdin input which we should remove first.
  fe.Inputs.clear(); 
  fe.Inputs.push_back
    (FrontendInputFile(source_file_name, clang::IK_OpenCL));
  fe.OutputFile = std::string(binary_file_name);

  CodeGenOptions &cg = pocl_build.getCodeGenOpts();
  cg.EmitOpenCLArgMetadata = 1;

  // TODO: use pch: it is possible to disable the strict checking for
  // the compilation flags used to compile it and the current translation
  // unit via the preprocessor options directly.

  // TODO: switch to EmitLLVMOnlyAction, when intermediate file is not needed
  // Do not give the global context to EmitBCAction as that leads to the
  // image types clashing and a running number appended to them whenever a
  // new module with the opaque type is reloaded.
  CodeGenAction *action = new clang::EmitBCAction();
  bool success = CI.ExecuteAction(*action);
  return success ? CL_SUCCESS : CL_BUILD_PROGRAM_FAILURE;
}

/* Retrieve metadata of the given kernel in the program to populate the
 * cl_kernel object.
 */
int call_pocl_kernel(cl_program program, 
                     cl_kernel kernel,
                     int device_i,     
                     const char* kernel_name,
                     const char* device_tmpdir, 
                     char* descriptor_filename,
                     int *errcode)
{

  int error, i;
  unsigned n;
  llvm::Module *input;
  SMDiagnostic Err;
  FILE *binary_file;
  char binary_filename[POCL_FILENAME_LENGTH];
  char tmpdir[POCL_FILENAME_LENGTH];

  assert(program->devices[device_i]->llvm_target_triplet && 
         "Device has no target triple set"); 

  snprintf (tmpdir, POCL_FILENAME_LENGTH, "%s/%s", 
            device_tmpdir, kernel_name);
  mkdir (tmpdir, S_IRWXU);

  error = snprintf(binary_filename, POCL_FILENAME_LENGTH,
                   "%s/kernel.bc",
                   tmpdir);
  error |= snprintf(descriptor_filename, POCL_FILENAME_LENGTH,
                   "%s/%s/descriptor.so", device_tmpdir, kernel_name);

  binary_file = fopen(binary_filename, "w+");
  if (binary_file == NULL)
    return (CL_OUT_OF_HOST_MEMORY);

  // TODO: dump the .bc only if we are using the kernel compiler
  // cache (POCL_LEAVE_TEMP_DIRS). Otherwise store a Module*
  // at clBuildProgram and reuse it here.
  n = fwrite(program->binaries[device_i], 1,
             program->binary_sizes[device_i], binary_file);
  if (n < program->binary_sizes[device_i])
    return (CL_OUT_OF_HOST_MEMORY);
  fclose(binary_file); 

  // Create own temporary context so we do not get duplicate image and sampler
  // opaque types to the context. They will get running numbers and
  // the name does not match anymore. 
  LLVMContext context;
  input = ParseIRFile(binary_filename, Err, context);
  if(!input) 
    {
      // TODO:
      raw_os_ostream os(std::cout);
      Err.print("pocl error: bad kernel file ", os);
      os.flush();
      exit(1);
    }
  
  PassManager Passes;
  DataLayout *TD = 0;
  const std::string &ModuleDataLayout = input->getDataLayout();
  if (!ModuleDataLayout.empty())
    TD = new DataLayout(ModuleDataLayout);

  llvm::Function *kernel_function = input->getFunction(kernel_name);
  assert(kernel_function && "TODO: make better check here");

  const llvm::Function::ArgumentListType &arglist = 
      kernel_function->getArgumentList();
  kernel->num_args=arglist.size();

  // This is from GenerateHeader.cc
  SmallVector<GlobalVariable *, 8> locals;
  for (llvm::Module::global_iterator i = input->global_begin(),
         e = input->global_end();
       i != e; ++i) {
    std::string funcName = "";
    funcName = kernel_function->getName().str();
    if (pocl::is_automatic_local(funcName, *i))
      {
        locals.push_back(i);
      }
  }

  kernel->num_locals = locals.size();

  /* This is from clCreateKernel.c */
  /* Temporary store for the arguments that are set with clSetKernelArg. */
  kernel->dyn_arguments =
    (struct pocl_argument *) malloc ((kernel->num_args + kernel->num_locals) *
                                     sizeof (struct pocl_argument));
  /* Initialize kernel "dynamic" arguments (in case the user doesn't). */
  for (unsigned i = 0; i < kernel->num_args; ++i)
    {
      kernel->dyn_arguments[i].value = NULL;
      kernel->dyn_arguments[i].size = 0;
    }

  /* Fill up automatic local arguments. */
  for (unsigned i = 0; i < kernel->num_locals; ++i)
    {
      unsigned auto_local_size = 
        TD->getTypeAllocSize(locals[i]->getInitializer()->getType());
      kernel->dyn_arguments[kernel->num_args + i].value = NULL;
      kernel->dyn_arguments[kernel->num_args + i].size = auto_local_size;
#ifdef DEBUG_POCL_LLVM_API
      printf("### automatic local %d size %u\n", i, auto_local_size);
#endif
    }

  // TODO: if the scripts are dumped, consider just having one list of 
  // enumerations, declaring what sort the arguments are
  kernel->arg_is_pointer = (cl_int*)malloc( sizeof(cl_int)*kernel->num_args );
  kernel->arg_is_local = (cl_int*)malloc( sizeof(cl_int)*kernel->num_args );
  kernel->arg_is_image = (cl_int*)malloc( sizeof(cl_int)*kernel->num_args );
  kernel->arg_is_sampler = (cl_int*)malloc( sizeof(cl_int)*kernel->num_args );
  
  // This is from GenerateHeader.cc
  i=0;
  for( llvm::Function::const_arg_iterator ii = arglist.begin(), 
                                          ee = arglist.end(); 
       ii != ee ; ii++)
  {
    Type *t = ii->getType();
  
    kernel->arg_is_image[i] = false;
    kernel->arg_is_sampler[i] = false;
 
    const PointerType *p = dyn_cast<PointerType>(t);
    if (p && !ii->hasByValAttr()) {
      kernel->arg_is_pointer[i] = true;
      // index 0 is for function attributes, parameters start at 1.
      if (p->getAddressSpace() == POCL_ADDRESS_SPACE_GLOBAL ||
          p->getAddressSpace() == POCL_ADDRESS_SPACE_CONSTANT ||
          pocl::is_image_type(*t) || pocl::is_sampler_type(*t))
        {
          kernel->arg_is_local[i] = false;
        }
      else
        {
          if (p->getAddressSpace() != POCL_ADDRESS_SPACE_LOCAL)
            {
              p->dump();
              assert(p->getAddressSpace() == POCL_ADDRESS_SPACE_LOCAL);
            }
          kernel->arg_is_local[i] = true;
        }
    } else {
      kernel->arg_is_pointer[i] = false;
      kernel->arg_is_local[i] = false;
    }

    if (pocl::is_image_type(*t))
      {
        kernel->arg_is_image[i] = true;
        kernel->arg_is_pointer[i] = false;
      } 
    else if (pocl::is_sampler_type(*t)) 
      {
        kernel->arg_is_sampler[i] = true;
        kernel->arg_is_pointer[i] = false;
      }
    i++;  
  }
  
  // fill 'kernel->reqd_wg_size'
  kernel->reqd_wg_size = (int*)malloc(3*sizeof(int));

  unsigned reqdx = 0, reqdy = 0, reqdz = 0;

  llvm::NamedMDNode *size_info = 
    kernel_function->getParent()->getNamedMetadata("opencl.kernel_wg_size_info");
  if (size_info) {
    for (unsigned i = 0, e = size_info->getNumOperands(); i != e; ++i) {
      llvm::MDNode *KernelSizeInfo = size_info->getOperand(i);
      if (KernelSizeInfo->getOperand(0) == kernel_function) {
        reqdx = (llvm::cast<ConstantInt>
                 (KernelSizeInfo->getOperand(1)))->getLimitedValue();
        reqdy = (llvm::cast<ConstantInt>
                 (KernelSizeInfo->getOperand(2)))->getLimitedValue();
        reqdz = (llvm::cast<ConstantInt>
                 (KernelSizeInfo->getOperand(3)))->getLimitedValue();
      }
    }
  }
  kernel->reqd_wg_size[0] = reqdx;
  kernel->reqd_wg_size[1] = reqdy;
  kernel->reqd_wg_size[2] = reqdz;
  
  // Generate the kernel_obj.c file. This should be optional
  // and generated only for the heterogeneous devices which need
  // these definitions to accompany the kernels, for the launcher
  // code.
  // TODO: the scripts use a generated kernel.h header file that
  // gets added to this file. No checks seem to fail if that file
  // is missing though, so it is left out from there for now
  std::string kobj_s = descriptor_filename; 
  kobj_s += ".kernel_obj.c"; 
  FILE *kobj_c = fopen( kobj_s.c_str(), "wc");
 
  fprintf(kobj_c, "\n #include <pocl_device.h>\n");

  fprintf(kobj_c,
    "void _%s_workgroup(void** args, struct pocl_context*);\n", kernel_name);
  fprintf(kobj_c,
    "void _%s_workgroup_fast(void** args, struct pocl_context*);\n", kernel_name);

  fprintf(kobj_c,
    "__attribute__((address_space(3))) __kernel_metadata _%s_md = {\n", kernel_name);
  fprintf(kobj_c,
    "     \"%s\", /* name */ \n", kernel_name );
  fprintf(kobj_c,"     %d, /* num_args */\n", kernel->num_args);
  fprintf(kobj_c,"     %d, /* num_locals */\n", kernel->num_locals);
#if 0
  // These are not used anymore. The launcher knows the arguments
  // and sets them up, the device just obeys and launches with
  // whatever arguments it gets. Remove if none of the private
  // branches need them neither.
  fprintf( kobj_c," #if _%s_NUM_LOCALS != 0\n",   kernel_name  );
  fprintf( kobj_c,"     _%s_LOCAL_SIZE,\n",       kernel_name  );
  fprintf( kobj_c," #else\n"    );
  fprintf( kobj_c,"     {0}, \n"    );
  fprintf( kobj_c," #endif\n"    );
  fprintf( kobj_c,"     _%s_ARG_IS_LOCAL,\n",    kernel_name  );
  fprintf( kobj_c,"     _%s_ARG_IS_POINTER,\n",  kernel_name  );
  fprintf( kobj_c,"     _%s_ARG_IS_IMAGE,\n",    kernel_name  );
  fprintf( kobj_c,"     _%s_ARG_IS_SAMPLER,\n",  kernel_name  );
#endif
  fprintf( kobj_c,"     _%s_workgroup_fast\n",   kernel_name  );
  fprintf( kobj_c," };\n");
  fclose(kobj_c);
  
  return 0;
  
}

/* kludge - this is the kernel dimensions command-line parameter to the workitem loop */
namespace pocl {
extern llvm::cl::list<int> LocalSize;
} 

#ifdef INCLUDE_UNFINISHED
/* This function links the input kernel LLVM bitcode and the
 * OpenCL kernel runtime library into one LLVM module, then
 * runs pocl's LLVM passes on that module.
 * Output is a LLVM bitcode file.
 */
int call_pocl_workgroup( char* function_name, 
                    size_t local_x, size_t local_y, size_t local_z,
                    const char* llvm_target_triplet, 
                    const char* parallel_filename,
                    const char* kernel_filename )
{

  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  std::string errmsg;
  StringMap<llvm::cl::Option*> opts;
  llvm::cl::getRegisteredOptions(opts);

  // TODO: do this globally, and just once per program
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeCore(Registry);
  initializeScalarOpts(Registry);
  initializeVectorization(Registry);
  initializeIPO(Registry);
  initializeAnalysis(Registry);
  initializeIPA(Registry);
  initializeTransformUtils(Registry);
  initializeInstCombine(Registry);
  initializeInstrumentation(Registry);
  initializeTarget(Registry);

  // FIXME: this too should be done only once!
  pocl::LocalSize.addValue(local_x);
  pocl::LocalSize.addValue(local_y);
  pocl::LocalSize.addValue(local_z);


  //TODO sync with Nat Ferrus' improved linking
  std::string kernellib;
  if (getenv("POCL_BUILDING") != NULL)
  {
    kernellib =BUILDDIR;
    kernellib+="/lib/kernel/kernel-";
    kernellib+=OCL_KERNEL_TARGET;
    kernellib+=".bc";   
  }
  else
  {
    // TODO: vefify this is the correct place!
    kernellib = PKGDATADIR;
    kernellib+="/pocl/";
    kernellib+=KERNEL_DIR;
    kernellib+="/kernel-";
    kernellib+=OCL_KERNEL_TARGET;
    kernellib+=".bc";
  }

  // Link the kernel and runtime library
  llvm::Module *input = ParseIRFile(kernel_filename, Err, Context);
  llvm::Module *libmodule = ParseIRFile(kernellib, Err, Context);
  Linker TheLinker( input );
  TheLinker.linkInModule( libmodule, &errmsg );
  llvm::Module *linked_bc = TheLinker.getModule();

  /* Start assembling the LLVM passes to run */
  PassManager Passes;
  DataLayout*TD = 0;
  const std::string &ModuleDataLayout = linked_bc->getDataLayout();
  if (!ModuleDataLayout.empty()) {
    TD = new DataLayout(ModuleDataLayout);
    Passes.add(TD);
  }
  else
  {
    // FIXME: panic more sublty
    assert( false );
  }

  /* The passes to run, in order */
  const char *passes[] = {"domtree", 
                          "workitem-handler-chooser",
                          "break-constgeps",
                          "automatic-locals",
                          "flatten", 
                          "always-inline",
                          "globaldce",
                          "simplifycfg",
                          "loop-simplify",
                          "phistoallocas",
                          "isolate-regions",
                          "uniformity",
                          "implicit-loop-barriers",
                          "loop-barriers", 
                          "barriertails",
                          "barriers",
                          "isolate-regions",
                          "wi-aa",
                          "workitemrepl", 
                          "workitemloops",
                          "allocastoentry",
                          "workgroup",
                          "target-address-spaces",
                          "STANDARD_OPTS",
                          "instcombine"}; 

  // Now add the above passes 
  for( unsigned i=0; i < sizeof(passes)/sizeof(const char*); i++ )
  {
    
    // This is (more or less) -O3
    if (strcmp("STANDARD_OPTS", passes[i])==0)
    {
      PassManagerBuilder Builder;
      Builder.OptLevel = 3;
      Builder.SizeLevel = 0;
      #if defined LLVM_3_2 or defined LLVM_3_3
      Builder.DisableSimplifyLibCalls=true;
      #endif
      Builder.populateModulePassManager( Passes );
      
      continue;
    }

    const PassInfo *PIs = Registry.getPassInfo(StringRef(passes[i]));
    if(PIs)
    {
      //std::cout << "-"<<passes[i] << " ";
      Pass *thispass = PIs->createPass();
      Passes.add(thispass);
    }
    else
    {
      // TODO: fail more gracefully.
      assert(false && "failed to create LLVM pass");
    }
  }

  llvm::cl::Option *O = opts["add-wi-metadata"];
  O->addOccurrence(1, StringRef("add-wi-metadata"), StringRef(""), false); 

  /* This is a beginning of the handling of the fine-tuning parameters.
   * Lots more needed...
   */
  std::string loopvec="";
  if (getenv("POCL_WORK_GROUP_METHOD") != NULL)
  {
    loopvec = getenv("POCL_WORK_GROUP_METHOD");
  }
  if (loopvec == "loopvec")
  {
    llvm::cl::Option *O = opts["vectorize-loops"];
    assert(O && "could not find LLVM option 'vectorize-loops'");
    O->addOccurrence(1, StringRef("vectorize-loops"), StringRef(""), false); 
    
    O = opts["vectorizer-min-trip-count"];
    assert(O && "could not find LLVM option 'vectorizer-min-trip-count'");
    O->addOccurrence(1, StringRef("vectorizer-min-trip-count"), StringRef("1"), false); 
  } 

  /* Now finally run the set of passes assembled above */
  std::string ErrorInfo;
  tool_output_file *Out = new tool_output_file( parallel_filename, 
                                                ErrorInfo, 
                                                F_Binary);;
  Passes.add(createBitcodeWriterPass(Out->os()));
  Passes.run(*linked_bc);

  Out->keep();
  delete Out;

  return 0;
}
#endif
