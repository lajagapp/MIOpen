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
#include <miopen/activ.hpp>
#include <miopen/kernel_cache.hpp>
#include <miopen/mlo_internal.hpp>
#include <miopen/float_equal.hpp>

namespace miopen {

static bool IsPackedTensor(const std::vector<std::size_t>& strides, const std::vector<std::size_t>& lens)
{
		int acc_lens = 1;

		for (auto i = lens.size() - 1; i > 0; i--)
		{
			if (acc_lens != strides[i])
				return false;

			acc_lens *= lens[i];
		}

		return true;
}


miopenStatus_t ActivationDescriptor::Forward(Handle& handle,
                                             const void* alpha,
                                             const TensorDescriptor& xDesc,
                                             ConstData_t x,
                                             const void* beta,
                                             const TensorDescriptor& yDesc,
                                             Data_t y,
                                             size_t xOffset,
                                             size_t yOffset)
{

    if(!float_equal(*(static_cast<const float*>(alpha)), 1.0) ||
       !float_equal(*(static_cast<const float*>(beta)), 0))
    {
        MIOPEN_THROW("Only alpha=1 and beta=0 is supported");
    }
    miopenStatus_t status = miopenStatusSuccess;
	mlo_construct_neuron construct_params(1); // forward

	double activ_alpha = GetAlpha();
	double activ_beta = GetBeta();
	double activ_power = GetPower();

	std::string network_config;
	construct_params.mloBuildConf_Key(network_config);
	size_t elem_space = xDesc.GetElementSpace();
	size_t elem_size = xDesc.GetElementSize();


	bool t2D = (xDesc.GetLengths().size() == yDesc.GetLengths().size() && xDesc.GetLengths().size() == 2);
	bool packed = IsPackedTensor(xDesc.GetStrides(), xDesc.GetLengths()) && IsPackedTensor(yDesc.GetStrides(), yDesc.GetLengths());

	if (xDesc.GetElementSize() == yDesc.GetElementSize() && (packed || t2D))
	{
		std::string compiler_options;
		size_t read_unit = (xDesc.GetElementSize() % 4 == 0) ? 4 : (xDesc.GetElementSize() % 2 == 0) ? 2 : 1;


		size_t MAP_RD = xDesc.GetElementSize() / read_unit;

		const std::string READ_TYPE = (read_unit == 1) ? "_FLOAT" : "_FLOAT" + std::to_string(read_unit);

		std::string type_opt;
		if (xDesc.GetType() == miopenFloat)
		{
			type_opt = " -DMIOPEN_USE_FP16=0 -DMIOPEN_USE_FP32=1";
		}
		else if (xDesc.GetType() == miopenHalf)
		{
			type_opt = " -DMIOPEN_USE_FP16=1 -DMIOPEN_USE_FP32=0";
		}

		compiler_options = " -DLITE -DMLO_READ_UNIT=" + std::to_string(read_unit) + " -DMLO_READ_TYPE=" + READ_TYPE + " -DMLO_NRN_OP_ID=" + std::to_string(mode) + type_opt;

		float f_activ_alpha = static_cast<float>(activ_alpha);
		float f_activ_beta = static_cast<float>(activ_beta);
		float f_activ_power = static_cast<float>(activ_power);

		std::vector<size_t> vld;
		std::vector<size_t> vgd;

		vld.push_back(256);
		vld.push_back(1);
		vld.push_back(1);

		vgd.push_back(MAP_RD);
		vgd.push_back(1);
		vgd.push_back(1);

		std::string program_name = "MIOpenNeuron.cl";
		std::string kernel_name = "MIOpenActiveFwdLite";
		handle.AddKernel("miopenActivationForward",
			network_config,
			program_name,
			kernel_name,
			vld,
			vgd,
			compiler_options)(
				x, y, f_activ_power, f_activ_beta, f_activ_alpha);

	}
	else
	{
		construct_params.setStream(&handle);

		int nOut = 1;
		int cOut = 1;
		int hOut = 1;
		int wOut = 1;
		int nOutStride = 0;
		int cOutStride = 0;
		int hOutStride = 0;
		int wOutStride = 0;

		if (yDesc.GetSize() == 4)
		{
			std::tie(nOut, cOut, hOut, wOut) = tien<4>(yDesc.GetLengths());
			std::tie(nOutStride, cOutStride, hOutStride, wOutStride) = tien<4>(yDesc.GetStrides());
		}
		else if (yDesc.GetSize() < 4 && yDesc.GetSize() > 0)
		{
			auto tensor_size = yDesc.GetSize();
			switch (tensor_size)
			{
			case 1:
				std::tie(wOut) = tien<1>(yDesc.GetLengths());
				std::tie(wOutStride) = tien<1>(yDesc.GetStrides());
				nOutStride = wOut * wOutStride;
				cOutStride = wOut * wOutStride;
				hOutStride = wOut * wOutStride;
				break;
			case 2:
				std::tie(hOut, wOut) = tien<2>(yDesc.GetLengths());
				std::tie(hOutStride, wOutStride) = tien<2>(yDesc.GetStrides());
				nOutStride = hOut * hOutStride;
				cOutStride = hOut * hOutStride;
				break;
			case 3:
				std::tie(cOut, hOut, wOut) = tien<3>(yDesc.GetLengths());
				std::tie(cOutStride, hOutStride, wOutStride) = tien<3>(yDesc.GetStrides());
				nOutStride = cOut * cOutStride;
				break;
			}
		}
		else
		{
			MIOPEN_THROW("activation does not support tensor size larger than 4 or smaller than 1");
		}

		construct_params.setTopDescr(
			"NCHW", "FP32", nOut, cOut, hOut, wOut, nOutStride, cOutStride, hOutStride, wOutStride);
		int nIn = 1;
		int cIn = 1;
		int hIn = 1;
		int wIn = 1;
		int nInStride = 0;
		int cInStride = 0;
		int hInStride = 0;
		int wInStride = 0;

		if (xDesc.GetSize() == 4)
		{
			std::tie(nIn, cIn, hIn, wIn) = tien<4>(xDesc.GetLengths());
			std::tie(nInStride, cInStride, hInStride, wInStride) = tien<4>(xDesc.GetStrides());
		}
		else if (xDesc.GetSize() < 4 && xDesc.GetSize() > 0)
		{
			auto tensor_size = xDesc.GetSize();
			switch (tensor_size)
			{
			case 1:
				std::tie(wIn) = tien<1>(xDesc.GetLengths());
				std::tie(wInStride) = tien<1>(xDesc.GetStrides());
				nInStride = wIn * wInStride;
				cInStride = wIn * wInStride;
				hInStride = wIn * wInStride;
				break;
			case 2:
				std::tie(hIn, wIn) = tien<2>(xDesc.GetLengths());
				std::tie(hInStride, wInStride) = tien<2>(xDesc.GetStrides());
				nInStride = hIn * hInStride;
				cInStride = hIn * hInStride;
				break;
			case 3:
				std::tie(cIn, hIn, wIn) = tien<3>(xDesc.GetLengths());
				std::tie(cInStride, hInStride, wInStride) = tien<3>(xDesc.GetStrides());
				nInStride = cIn * cInStride;
				break;
			}
		}
		else
		{
			MIOPEN_THROW(
				"Activation does not support tensor dimension larger than 4 or smaller than 1");
		}

		construct_params.setBotDescFromMLDesc(xDesc);



		construct_params.setNeuronDescr(static_cast<int>(mode), activ_power, activ_beta, activ_alpha);

		mloConstruct(construct_params);

		std::string program_name = construct_params.getKernelFile();      // CL kernel filename
		std::string kernel_name = construct_params.getKernelName();      // kernel name
		std::string compiler_options = construct_params.getCompilerOptions(); // kernel parameters


		const std::vector<size_t>& vld = construct_params.getLocalWkSize();
		const std::vector<size_t>& vgd = construct_params.getGlobalWkSize();

		int imode = mode;
		construct_params.getNeuronDescr(imode, activ_power, activ_beta, activ_alpha);

		auto f_activ_alpha = static_cast<float>(activ_alpha);
		auto f_activ_beta = static_cast<float>(activ_beta);
		auto f_activ_power = static_cast<float>(activ_power);

		compiler_options +=
			" -DMLO_N_IN=" + std::to_string(nIn) + " -DMLO_C_IN=" + std::to_string(cIn) +
			" -DMLO_H_IN=" + std::to_string(hIn) + " -DMLO_W_IN=" + std::to_string(wIn) +
			" -DMLO_N_IN_STRIDE=" + std::to_string(nInStride) + " -DMLO_C_IN_STRIDE=" +
			std::to_string(cInStride) + " -DMLO_H_IN_STRIDE=" + std::to_string(hInStride) +
			" -DMLO_W_IN_STRIDE=" + std::to_string(wInStride) + " -DMLO_N_OUT=" + std::to_string(nOut) +
			" -DMLO_C_OUT=" + std::to_string(cOut) + " -DMLO_H_OUT=" + std::to_string(hOut) +
			" -DMLO_W_OUT=" + std::to_string(wOut) + " -DMLO_N_OUT_STRIDE=" +
			std::to_string(nOutStride) + " -DMLO_C_OUT_STRIDE=" + std::to_string(cOutStride) +
			" -DMLO_H_OUT_STRIDE=" + std::to_string(hOutStride) + " -DMLO_W_OUT_STRIDE=" +
			std::to_string(wOutStride) + " -DMLO_N_DIN=" + std::to_string(1) + " -DMLO_C_DIN=" +
			std::to_string(1) + " -DMLO_H_DIN=" + std::to_string(1) + " -DMLO_W_DIN=" +
			std::to_string(1) + " -DMLO_N_DIN_STRIDE=" + std::to_string(1) + " -DMLO_C_DIN_STRIDE=" +
			std::to_string(1) + " -DMLO_H_DIN_STRIDE=" + std::to_string(1) + " -DMLO_W_DIN_STRIDE=" +
			std::to_string(1) + " -DMLO_N_DOUT=" + std::to_string(1) + " -DMLO_C_DOUT=" +
			std::to_string(1) + " -DMLO_H_DOUT=" + std::to_string(1) + " -DMLO_W_DOUT=" +
			std::to_string(1) + " -DMLO_N_DOUT_STRIDE=" + std::to_string(1) + " -DMLO_C_DOUT_STRIDE=" +
			std::to_string(1) + " -DMLO_H_DOUT_STRIDE=" + std::to_string(1) + " -DMLO_W_DOUT_STRIDE=" +
			std::to_string(1) + " -DMLO_IN_BLOCK_SZ=" + std::to_string(cIn * hIn * wIn) +
			" -DMLO_OUT_BLOCK_SZ=" + std::to_string(cOut * hOut * wOut) + " -DMLO_DIN_BLOCK_SZ=" +
			std::to_string(1) + " -DMLO_DOUT_BLOCK_SZ=" + std::to_string(1);

		handle.AddKernel("miopenActivationForward",
			network_config,
			program_name,
			kernel_name,
			vld,
			vgd,
			compiler_options)(
				x, y, f_activ_power, f_activ_beta, f_activ_alpha, cl_long(xOffset), cl_long(yOffset));
	}
    return (status);
}

miopenStatus_t ActivationDescriptor::Backward(Handle& handle,
                                              const void* alpha,
                                              const TensorDescriptor& yDesc,
                                              ConstData_t y,
                                              const TensorDescriptor& dyDesc,
                                              ConstData_t dy,
                                              const TensorDescriptor& xDesc,
                                              ConstData_t x,
                                              const void* beta,
                                              const TensorDescriptor& dxDesc,
                                              Data_t dx,
                                              size_t yOffset,
                                              size_t dyOffset,
                                              size_t xOffset,
                                              size_t dxOffset)
{

    if(!float_equal(*(static_cast<const float*>(alpha)), 1.0) ||
       !float_equal(*(static_cast<const float*>(beta)), 0))
    {
        MIOPEN_THROW("Only alpha=1 and beta=0 is supported");
    }
    miopenStatus_t status = miopenStatusSuccess;

    mlo_construct_neuron construct_params(0); // backward

	double activ_alpha = GetAlpha();
	double activ_beta = GetBeta();
	double activ_power = GetPower();

	std::string network_config;
	construct_params.mloBuildConf_Key(network_config);


	bool t2D = (xDesc.GetLengths().size() == yDesc.GetLengths().size() && xDesc.GetLengths().size() == 2);
	bool packed = IsPackedTensor(xDesc.GetStrides(), xDesc.GetLengths()) && IsPackedTensor(yDesc.GetStrides(), yDesc.GetLengths());

	if (xDesc.GetElementSize() == yDesc.GetElementSize() && (packed || t2D))
	{
		std::string compiler_options;

		size_t read_unit = (xDesc.GetElementSize() % 4 == 0) ? 4 : (xDesc.GetElementSize() % 2 == 0) ? 2 : 1;


		size_t MAP_RD = xDesc.GetElementSize() / read_unit;

		const std::string READ_TYPE = (read_unit == 1) ? "_FLOAT" : "_FLOAT" + std::to_string(read_unit);

		std::string type_opt;
		if (xDesc.GetType() == miopenFloat)
		{
			type_opt = " -DMIOPEN_USE_FP16=0 -DMIOPEN_USE_FP32=1";
		}
		else if (xDesc.GetType() == miopenHalf)
		{
			type_opt = " -DMIOPEN_USE_FP16=1 -DMIOPEN_USE_FP32=0";
		}

		compiler_options = " -DLITE -DMLO_READ_UNIT=" + std::to_string(read_unit) + " -DMLO_READ_TYPE=" + READ_TYPE + " -DMLO_NRN_OP_ID=" + std::to_string(mode) + type_opt;

		float f_activ_alpha = static_cast<float>(activ_alpha);
		float f_activ_beta = static_cast<float>(activ_beta);
		float f_activ_power = static_cast<float>(activ_power);
		float f_diff_scale = f_activ_beta * f_activ_power;

		std::vector<size_t> vld;
		std::vector<size_t> vgd;

		vld.push_back(256);
		vld.push_back(1);
		vld.push_back(1);

		vgd.push_back(MAP_RD);
		vgd.push_back(1);
		vgd.push_back(1);


		std::string program_name = "MIOpenNeuron.cl";
		std::string kernel_name = "MIOpenActiveBwdLite";
		handle.AddKernel("miopenActivationBackward",
			network_config,
			program_name,
			kernel_name,
			vld,
			vgd,
			compiler_options)(dx,
				dy,
				x,
				y,
				f_diff_scale,
				f_activ_power,
				f_activ_beta,
				f_activ_alpha);

	}
	else
	{
		construct_params.setStream(&handle);
		int ndOut = 1;
		int cdOut = 1;
		int hdOut = 1;
		int wdOut = 1;
		int ndOutStride = 0;
		int cdOutStride = 0;
		int hdOutStride = 0;
		int wdOutStride = 0;

		if (dyDesc.GetSize() == 4)
		{
			std::tie(ndOut, cdOut, hdOut, wdOut) = tien<4>(dyDesc.GetLengths());
			std::tie(ndOutStride, cdOutStride, hdOutStride, wdOutStride) = tien<4>(dyDesc.GetStrides());
		}
		else if (dyDesc.GetSize() < 4 && dyDesc.GetSize() > 0)
		{
			auto tensor_size = dyDesc.GetSize();
			switch (tensor_size)
			{
			case 1:
				std::tie(wdOut) = tien<1>(dyDesc.GetLengths());
				std::tie(wdOutStride) = tien<1>(dyDesc.GetStrides());
				ndOutStride = wdOut * wdOutStride;
				cdOutStride = wdOut * wdOutStride;
				hdOutStride = wdOut * wdOutStride;
				break;
			case 2:
				std::tie(hdOut, wdOut) = tien<2>(dyDesc.GetLengths());
				std::tie(hdOutStride, wdOutStride) = tien<2>(dyDesc.GetStrides());
				ndOutStride = hdOut * hdOutStride;
				cdOutStride = hdOut * hdOutStride;
				break;
			case 3:
				std::tie(cdOut, hdOut, wdOut) = tien<3>(dyDesc.GetLengths());
				std::tie(cdOutStride, hdOutStride, wdOutStride) = tien<3>(dyDesc.GetStrides());
				ndOutStride = cdOut * cdOutStride;
				break;
			}
		}
		else
		{
			MIOPEN_THROW("activation does not support tensor size larger than 4 or smaller than 1");
		}

		construct_params.setTopDfDescFromMLDesc(dyDesc);

		int nOut = 1;
		int cOut = 1;
		int hOut = 1;
		int wOut = 1;
		int nOutStride = 0;
		int cOutStride = 0;
		int hOutStride = 0;
		int wOutStride = 0;

		if (yDesc.GetSize() == 4)
		{
			std::tie(nOut, cOut, hOut, wOut) = tien<4>(yDesc.GetLengths());
			std::tie(nOutStride, cOutStride, hOutStride, wOutStride) = tien<4>(yDesc.GetStrides());
		}
		else if (yDesc.GetSize() < 4 && yDesc.GetSize() > 0)
		{
			auto tensor_size = yDesc.GetSize();
			switch (tensor_size)
			{
			case 1:
				std::tie(wOut) = tien<1>(yDesc.GetLengths());
				std::tie(wOutStride) = tien<1>(yDesc.GetStrides());
				nOutStride = wOut * wOutStride;
				cOutStride = wOut * wOutStride;
				hOutStride = wOut * wOutStride;
				break;
			case 2:
				std::tie(hOut, wOut) = tien<2>(yDesc.GetLengths());
				std::tie(hOutStride, wOutStride) = tien<2>(yDesc.GetStrides());
				nOutStride = hOut * hOutStride;
				cOutStride = hOut * hOutStride;
				break;
			case 3:
				std::tie(cOut, hOut, wOut) = tien<3>(yDesc.GetLengths());
				std::tie(cOutStride, hOutStride, wOutStride) = tien<3>(yDesc.GetStrides());
				nOutStride = cOut * cOutStride;
				break;
			}
		}
		else
		{
			MIOPEN_THROW(
				"Activation does not support tensor dimensions larger than 4 or smaller than 1");
		}

		construct_params.setTopDescFromMLDesc(yDesc);

		int ndIn = 1;
		int cdIn = 1;
		int hdIn = 1;
		int wdIn = 1;
		int ndInStride = 0;
		int cdInStride = 0;
		int hdInStride = 0;
		int wdInStride = 0;

		if (dxDesc.GetSize() == 4)
		{
			std::tie(ndIn, cdIn, hdIn, wdIn) = tien<4>(dxDesc.GetLengths());
			std::tie(ndInStride, cdInStride, hdInStride, wdInStride) = tien<4>(dxDesc.GetStrides());
		}
		else if (dxDesc.GetSize() < 4 && dxDesc.GetSize() > 0)
		{
			auto tensor_size = dxDesc.GetSize();
			switch (tensor_size)
			{
			case 1:
				std::tie(wdIn) = tien<1>(dxDesc.GetLengths());
				std::tie(wdInStride) = tien<1>(dxDesc.GetStrides());
				ndInStride = wdIn * wdInStride;
				cdInStride = wdIn * wdInStride;
				hdInStride = wdIn * wdInStride;
				break;
			case 2:
				std::tie(hdIn, wdIn) = tien<2>(dxDesc.GetLengths());
				std::tie(hdInStride, wdInStride) = tien<2>(dxDesc.GetStrides());
				ndInStride = hdIn * hdInStride;
				cdInStride = hdIn * hdInStride;
				break;
			case 3:
				std::tie(cdIn, hdIn, wdIn) = tien<3>(dxDesc.GetLengths());
				std::tie(cdInStride, hdInStride, wdInStride) = tien<3>(dxDesc.GetStrides());
				ndInStride = cdIn * cdInStride;
				break;
			}
		}
		else
		{
			MIOPEN_THROW(
				"Activation does not support tensor dimensions larger than 4 or smaller than 1");
		}

		construct_params.setBotDfDescFromMLDesc(dxDesc);

		int nIn = 1;
		int cIn = 1;
		int hIn = 1;
		int wIn = 1;
		int nInStride = 0;
		int cInStride = 0;
		int hInStride = 0;
		int wInStride = 0;

		if (xDesc.GetSize() == 4)
		{
			std::tie(nIn, cIn, hIn, wIn) = tien<4>(xDesc.GetLengths());
			std::tie(nInStride, cInStride, hInStride, wInStride) = tien<4>(xDesc.GetStrides());
		}
		else if (xDesc.GetSize() < 4 && xDesc.GetSize() > 0)
		{
			auto tensor_size = xDesc.GetSize();
			switch (tensor_size)
			{
			case 1:
				std::tie(wIn) = tien<1>(xDesc.GetLengths());
				std::tie(wInStride) = tien<1>(xDesc.GetStrides());
				nInStride = wIn * wInStride;
				cInStride = wIn * wInStride;
				hInStride = wIn * wInStride;
				break;
			case 2:
				std::tie(hIn, wIn) = tien<2>(xDesc.GetLengths());
				std::tie(hInStride, wInStride) = tien<2>(xDesc.GetStrides());
				nInStride = hIn * hInStride;
				cInStride = hIn * hInStride;
				break;
			case 3:
				std::tie(cIn, hIn, wIn) = tien<3>(xDesc.GetLengths());
				std::tie(cInStride, hInStride, wInStride) = tien<3>(xDesc.GetStrides());
				nInStride = cIn * cInStride;
				break;
			}
		}
		else
		{
			MIOPEN_THROW(
				"Activation does not support tensor dimensions larger than 4 or smaller than 1");
		}

		construct_params.setBotDescFromMLDesc(xDesc);

		int activ_mode = GetMode();

		construct_params.setNeuronDescr(activ_mode, activ_power, activ_beta, activ_alpha);

		mloConstruct(construct_params);

		std::string program_name = construct_params.getKernelFile();      // CL kernel filename
		std::string kernel_name = construct_params.getKernelName();      // kernel name
		std::string compiler_options = construct_params.getCompilerOptions(); // kernel parameters

		const std::vector<size_t>& vld = construct_params.getLocalWkSize();
		const std::vector<size_t>& vgd = construct_params.getGlobalWkSize();

		auto f_activ_alpha = static_cast<float>(GetAlpha());
		auto f_activ_beta = static_cast<float>(GetBeta());
		auto f_activ_power = static_cast<float>(GetPower());
		float f_diff_scale = f_activ_beta * f_activ_power;

		compiler_options +=
			" -DMLO_N_IN=" + std::to_string(nIn) + " -DMLO_C_IN=" + std::to_string(cIn) +
			" -DMLO_H_IN=" + std::to_string(hIn) + " -DMLO_W_IN=" + std::to_string(wIn) +
			" -DMLO_N_IN_STRIDE=" + std::to_string(nInStride) + " -DMLO_C_IN_STRIDE=" +
			std::to_string(cInStride) + " -DMLO_H_IN_STRIDE=" + std::to_string(hInStride) +
			" -DMLO_W_IN_STRIDE=" + std::to_string(wInStride) + " -DMLO_N_OUT=" + std::to_string(nOut) +
			" -DMLO_C_OUT=" + std::to_string(cOut) + " -DMLO_H_OUT=" + std::to_string(hOut) +
			" -DMLO_W_OUT=" + std::to_string(wOut) + " -DMLO_N_OUT_STRIDE=" +
			std::to_string(nOutStride) + " -DMLO_C_OUT_STRIDE=" + std::to_string(cOutStride) +
			" -DMLO_H_OUT_STRIDE=" + std::to_string(hOutStride) + " -DMLO_W_OUT_STRIDE=" +
			std::to_string(wOutStride) + " -DMLO_N_DIN=" + std::to_string(ndIn) + " -DMLO_C_DIN=" +
			std::to_string(cdIn) + " -DMLO_H_DIN=" + std::to_string(hdIn) + " -DMLO_W_DIN=" +
			std::to_string(wdIn) + " -DMLO_N_DIN_STRIDE=" + std::to_string(ndInStride) +
			" -DMLO_C_DIN_STRIDE=" + std::to_string(cdInStride) + " -DMLO_H_DIN_STRIDE=" +
			std::to_string(hdInStride) + " -DMLO_W_DIN_STRIDE=" + std::to_string(wdInStride) +
			" -DMLO_N_DOUT=" + std::to_string(ndOut) + " -DMLO_C_DOUT=" + std::to_string(cdOut) +
			" -DMLO_H_DOUT=" + std::to_string(hdOut) + " -DMLO_W_DOUT=" + std::to_string(wdOut) +
			" -DMLO_N_DOUT_STRIDE=" + std::to_string(ndOutStride) + " -DMLO_C_DOUT_STRIDE=" +
			std::to_string(cdOutStride) + " -DMLO_H_DOUT_STRIDE=" + std::to_string(hdOutStride) +
			" -DMLO_W_DOUT_STRIDE=" + std::to_string(wdOutStride) + " -DMLO_IN_BLOCK_SZ=" +
			std::to_string(cIn * hIn * wIn) + " -DMLO_OUT_BLOCK_SZ=" +
			std::to_string(cOut * hOut * wOut) + " -DMLO_DIN_BLOCK_SZ=" +
			std::to_string(cdIn * hdIn * wdIn) + " -DMLO_DOUT_BLOCK_SZ=" +
			std::to_string(cdOut * hdOut * wdOut);

		handle.AddKernel("miopenActivationBackward",
			network_config,
			program_name,
			kernel_name,
			vld,
			vgd,
			compiler_options)(dx,
				dy,
				x,
				y,
				f_diff_scale,
				f_activ_power,
				f_activ_beta,
				f_activ_alpha,
				cl_long(dxOffset),
				cl_long(dyOffset),
				cl_long(xOffset),
				cl_long(yOffset));
	}
    return (status);
}
} // namespace miopen
