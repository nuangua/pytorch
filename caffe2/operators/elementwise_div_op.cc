#include "caffe2/operators/elementwise_div_op.h"

#include <algorithm>
#include <functional>

namespace caffe2 {

#if !CAFFE2_MOBILE

namespace {

template <typename TGrad, typename TIn, typename TOut>
void ComputeDivGradient(
    const int ndim,
    const int* A_dims,
    const int* B_dims,
    const int* C_dims,
    const TGrad* dC,
    const TIn* B,
    const TOut* C,
    TGrad* dA,
    TGrad* dB,
    CPUContext* context) {
  const int A_size =
      std::accumulate(A_dims, A_dims + ndim, 1, std::multiplies<int>());
  const int B_size =
      std::accumulate(B_dims, B_dims + ndim, 1, std::multiplies<int>());
  const int C_size =
      std::accumulate(C_dims, C_dims + ndim, 1, std::multiplies<int>());
  math::Set<TGrad, CPUContext>(A_size, TGrad(0), dA, context);
  math::Set<TGrad, CPUContext>(B_size, TGrad(0), dB, context);
  std::vector<int> index(ndim, 0);
  for (int C_index = 0; C_index < C_size; ++C_index) {
    const int A_index =
        math::utils::GetIndexFromDims(ndim, A_dims, index.data());
    const int B_index =
        math::utils::GetIndexFromDims(ndim, B_dims, index.data());
    dA[A_index] += dC[C_index] / B[B_index];
    dB[B_index] += -dC[C_index] * C[C_index] / B[B_index];
    math::utils::IncreaseIndexInDims(ndim, C_dims, index.data());
  }
}

} // namespace

template <>
template <typename TGrad, typename TIn, typename TOut>
bool DivFunctor<CPUContext>::Backward(
    const std::vector<int>& A_dims,
    const std::vector<int>& B_dims,
    const TGrad* dC,
    const TIn* /* A */,
    const TIn* B,
    const TOut* C,
    TGrad* dA,
    TGrad* dB,
    CPUContext* context) const {
  if (A_dims == B_dims) {
    const int size = std::accumulate(
        A_dims.cbegin(), A_dims.cend(), 1, std::multiplies<int>());
    math::Div(size, dC, B, dA, context);
    EigenVectorMap<TGrad>(dB, size) =
        -ConstEigenVectorArrayMap<TGrad>(dC, size) *
        ConstEigenVectorArrayMap<TOut>(C, size) /
        ConstEigenVectorArrayMap<TIn>(B, size);
    return true;
  }
  const int ndim = std::max(A_dims.size(), B_dims.size());
  std::vector<int> A_broadcast_dims(ndim);
  std::vector<int> B_broadcast_dims(ndim);
  std::vector<int> C_broadcast_dims(ndim);
  math::utils::ComputeBroadcastBinaryOpDims(
      A_dims.size(),
      A_dims.data(),
      B_dims.size(),
      B_dims.data(),
      A_broadcast_dims.data(),
      B_broadcast_dims.data(),
      C_broadcast_dims.data());
  ComputeDivGradient<TGrad, TIn, TOut>(
      ndim,
      A_broadcast_dims.data(),
      B_broadcast_dims.data(),
      C_broadcast_dims.data(),
      dC,
      B,
      C,
      dA,
      dB,
      context);
  return true;
}

#endif // !CAFFE2_MOBILE

REGISTER_CPU_OPERATOR(
    Div,
    BinaryElementwiseOp<NumericTypes, CPUContext, DivFunctor<CPUContext>>);

#if !CAFFE2_MOBILE

REGISTER_CPU_OPERATOR(
    DivGradient,
    BinaryElementwiseGradientOp<
        NumericTypes,
        CPUContext,
        DivFunctor<CPUContext>>);

namespace {

class GetDivGradient final : public GradientMakerBase {
  using GradientMakerBase::GradientMakerBase;

  std::vector<OperatorDef> GetGradientDefs() override {
    return SingleGradientDef(
        "DivGradient",
        "",
        std::vector<std::string>{GO(0), I(0), I(1), O(0)},
        std::vector<std::string>{GI(0), GI(1)});
  }
};

} // namespace

REGISTER_GRADIENT(Div, GetDivGradient);

#endif // !CAFFE2_MOBILE

} // namespace caffe2
