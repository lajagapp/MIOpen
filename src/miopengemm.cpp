/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include <regex>
#include <miopen/handle.hpp>
#include <miopen/miopengemm.hpp>

#define MIOPENGEMM_CPP_DEBUG 0

#if MIOPEN_USE_MIOPENGEMM
namespace miopen {

// so that MIOpen works whether or not recent MIOpenGEMM changes pulled:
// convert size_t and ulong kernel function parameters to unsigned.
namespace tempfix_v2 {
void set_offsets_to_uint(std::string& clstr)
{
    auto get_target = [](std::string inttype, char x) {
        std::stringstream ss;
        ss << "const " << inttype << ' ' << std::string(1, x) << "_offset";
        return std::regex(ss.str());
    };

    for(char x : {'a', 'b', 'c'})
    {
        std::string replacement = "const unsigned " + std::string(1, x) + "_offset";
        for(auto inttype : {"size_t", "ulong"})
        {
            clstr = std::regex_replace(clstr, get_target(inttype, x), replacement);
        }
    }
}
} // namespace tempfix_v2

// TODO: doesn't support offset to A, B, C yet
void AddMiopengemmSolution(Handle& handle,
                           const std::string& algorithm_name,
                           const std::string& network_config,
                           const MIOpenGEMM::Geometry& mgg,
                           ConstData_t A,
                           ConstData_t B,
                           Data_t C,
                           float time,
                           bool enforce_determinism)
{
#if MIOPEN_BACKEND_OPENCL
    // jn : print search results to terminal
    bool miopengemm_verbose = false;

    // jn : print warning messages when the returned kernel(s) might be sub-optimal
    bool miopengemm_warnings = false;

    // jn : find with no workspace
    MIOpenGEMM::Solution soln = MIOpenGEMM::find(time,
                                                 handle.GetStream(),
                                                 A,
                                                 B,
                                                 C,
                                                 enforce_determinism,
                                                 mgg,
                                                 miopengemm_verbose,
                                                 miopengemm_warnings);
#else
    (void)A;
    (void)B;
    (void)C;
    (void)time;
    (void)enforce_determinism;
    MIOpenGEMM::Solution soln = MIOpenGEMM::get_default(mgg);
#endif
    // jn : the main kernel is at the back of the solution vector
    std::string kernel_clstring = soln.v_tgks.back().kernstr;
    tempfix_v2::set_offsets_to_uint(kernel_clstring);

    std::string kernel_name = soln.v_tgks.back().fname;
    size_t local_work_size  = soln.v_tgks.back().local_work_size;
    size_t global_work_size = soln.v_tgks.back().global_work_size;

    std::vector<size_t> vld{local_work_size, 1, 1};
    std::vector<size_t> vgd{global_work_size, 1, 1};

    // chao : there are 2 possible kernel paths for C = alpha * A * B + beta * C in MIOpenGEMM
    // library
    //   1) kernel_0 : C = alpha * A * B + beta * C
    //   2) kernel_1 : C *= beta
    //      kernel_2 : C += alpha * A * B

    // this kernel could be kernel_0, kernel_1
    handle.AddKernel(algorithm_name, network_config, kernel_clstring, kernel_name, vld, vgd, "", 0);

    if(soln.v_tgks.size() == 2)
    {
        std::string beta_program_name = soln.v_tgks[0].kernstr;
        tempfix_v2::set_offsets_to_uint(beta_program_name);

        std::string beta_kernel_name = soln.v_tgks[0].fname;
        local_work_size              = soln.v_tgks[0].local_work_size;
        global_work_size             = soln.v_tgks[0].global_work_size;

        vld[0] = local_work_size;
        vgd[0] = global_work_size;

        // chao : this is kernel_2: C += alpha * A * B (needs to work with kernel_1)
        handle.AddKernel(
            algorithm_name, network_config, beta_program_name, beta_kernel_name, vld, vgd, "", 1);
    }

#if MIOPENGEMM_CPP_DEBUG
    {
        const auto& kernels = handle.GetKernels(algorithm_name, network_config);

        for(const auto& k : kernels)
            std::cout << __func__ << ": kernel name: " << k.GetName() << std::endl;

        if(kernels.size() == 2)
        {
            // C *= beta
            assert(kernels[1].GetName() == "miog_betac");

            // C += alpha * A * B
            assert(kernels[0].GetName() == "miog_alphaab");
        }
        else if(kernels.size() == 1)
        {
            // C = alpha * A * B + beta * C
            assert(kernels[0].GetName() == "miog_betac_alphaab");
        }
        else
        {
            MIOPEN_THROW("unable to get correct MIOpenGEMM kenerls");
        }
    }
#endif
}

void RunMiopengemmSolution(Handle& handle,
                           const std::string& algorithm_name,
                           const std::string& network_config,
                           float alpha,
                           ConstData_t A,
                           int a_offset,
                           ConstData_t B,
                           int b_offset,
                           float beta,
                           Data_t C,
                           int c_offset)
{
    const auto& kernels = handle.GetKernels(algorithm_name, network_config);

#if MIOPENGEMM_CPP_DEBUG
    {
        for(const auto& k : kernels)
            std::cout << __func__ << ": kernel name: " << k.GetName() << std::endl;
    }
#endif

    if(kernels.size() == 2)
    {
// C *= beta
#if MIOPENGEMM_CPP_DEBUG
        assert(kernels[1].GetName() == "miog_betac");
#endif
        kernels[1](C, c_offset, beta);

// C += alpha * A * B
#if MIOPENGEMM_CPP_DEBUG
        assert(kernels[0].GetName() == "miog_alphaab");
#endif
        kernels[0](A, a_offset, B, b_offset, C, c_offset, alpha);
    }
    else if(kernels.size() == 1)
    {
// C = alpha * A * B + beta * C
#if MIOPENGEMM_CPP_DEBUG
        assert(kernels[0].GetName() == "miog_betac_alphaab");
#endif
        kernels[0](A, a_offset, B, b_offset, C, c_offset, alpha, beta);
    }
    else
    {
        MIOPEN_THROW("unable to get correct MIOpenGEMM kenerls");
    }
}

} // namespace miopen
#endif // MIOPEN_USE_MIOPENGEMM
