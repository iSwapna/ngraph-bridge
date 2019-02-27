/*******************************************************************************
 * Copyright 2017-2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "tensorflow/core/common_runtime/dma_helper.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/lib/strings/strcat.h"

#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/platform/default/logging.h"
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"

#include "ngraph/runtime/backend.hpp"
#include "ngraph_backend_manager.h"
#include "ngraph_freshness_tracker.h"
#include "ngraph_utils.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

/* -------------------------------------------------
//
// NGraphAssignOp
//
---------------------------------------------------*/

// Computes *input[0] = input[1]
class NGraphAssignOp : public OpKernel {
 private:
  bool just_looking_;
  bool copy_to_tf_;
  // bool use_exclusive_lock_;
  // bool validate_shape_;
  // bool relax_constraints_;

 public:
  explicit NGraphAssignOp(OpKernelConstruction* context)
      : OpKernel(context), just_looking_(false), copy_to_tf_(false) {
    // OP_REQUIRES_OK(context,
    //                context->GetAttr("use_locking", &use_exclusive_lock_));
    // OP_REQUIRES_OK(context,
    //                context->GetAttr("validate_shape", &validate_shape_));
    OP_REQUIRES_OK(context, context->GetAttr("just_looking", &just_looking_));
    OP_REQUIRES_OK(context, context->GetAttr("copy_to_tf", &copy_to_tf_));
    NGRAPH_VLOG(1) << "Constructor " << def().name() << ": just looking? "
                   << just_looking_ << " ,copy-to-tf " << copy_to_tf_;
    OP_REQUIRES(context, IsRefType(context->input_type(0)),
                errors::InvalidArgument("lhs input needs to be a ref type"));
    // if (!context
    //          ->GetAttr("_grappler_relax_allocator_constraints",
    //                    &relax_constraints_)
    //          .ok()) {
    //   relax_constraints_ = false;
    // }
  }

  void Compute(OpKernelContext* context) override {
    NGRAPH_VLOG(1)<< "In Assign Kernel "<< def().name();
    NGRAPH_VLOG(1)<<"Copy to TF "<< PrintBool(copy_to_tf_);
    NGRAPH_VLOG(1)<<"Just Looking " << PrintBool(just_looking_);

    const Tensor& rhs = context->input(1);

    // We always return the input ref.
    context->forward_ref_input_to_ref_output(0, 0);

    // get the nGraphTensor
    // string ng_variable_name = context->
    shared_ptr<ngraph::runtime::Tensor> ng_tensor_to_assign =
        BackendManager::ng_variable_map_["Var1"];

    void* tf_src_ptr = (void*)DMAHelper::base(&rhs);
    ng_tensor_to_assign->write(
        tf_src_ptr, 0, ng_tensor_to_assign->get_element_count() *
                           ng_tensor_to_assign->get_element_type().size());

    NGRAPH_VLOG(1) << " Print NG Tensor ";
    PrintNGTensor(ng_tensor_to_assign);
    
    mutex_lock l(*context->input_ref_mutex(0));
    Tensor old_lhs = context->mutable_input(0, /* lock_held */ true);

    NGRAPH_VLOG(1) << " Print TF Tensor ";
    PrintTFTensor(old_lhs);

    if (copy_to_tf_) {
      // update the tf tensor
      //mutex_lock l(*context->input_ref_mutex(0));
      // const Tensor& old_lhs = context->mutable_input(0, /* lock_held */
      // true);
      //Tensor old_lhs = context->mutable_input(0, /* lock_held */ true);
      ReadNGTensor(ng_tensor_to_assign, &old_lhs);
      NGRAPH_VLOG(1) << "Copying to TF Tensor";
      NGRAPH_VLOG(1) << "Print ng-tensor";
      PrintNGTensor(ng_tensor_to_assign);

      NGRAPH_VLOG(1) << "Print tf-tensor";
      PrintTFTensor(old_lhs);

      if (just_looking_) {
        // Some tf op will just use the val

      } else {
        // Some tf op might update the ng-tensor value so mark it stale
      }
    }
  }
};

REGISTER_OP("NGraphAssign")
    .Input("ref: Ref(T)")
    .Input("value: T")
    .Output("output_ref: Ref(T)")
    .Attr("T: type")
    .Attr("validate_shape: bool = true")
    .Attr("use_locking: bool = true")
    .Attr("just_looking: bool = false")
    .Attr("copy_to_tf: bool = false");

REGISTER_KERNEL_BUILDER(Name("NGraphAssign").Device(DEVICE_CPU),
                        NGraphAssignOp);

}  // namespace ngraph_bridge

}  // namespace tensorflow