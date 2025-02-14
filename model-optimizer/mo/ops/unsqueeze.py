# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from mo.front.common.partial_infer.utils import int64_array, shape_array, is_fully_defined, shape_insert
from mo.graph.perm_inputs import PermuteInputs
from mo.ops.op import Op
from mo.utils.error import Error


class Unsqueeze(Op):
    """
    The operation that inserts dimensions of size one into specific positions of the input layer. The dimensions are
    specified in the second input.
    """
    op = 'Unsqueeze'
    enabled = False

    def __init__(self, graph, attrs: dict):
        super().__init__(graph, {
            'op': self.op,
            'type': self.op,
            'version': 'opset1',
            'unsqueeze_dims': None,
            'reinterp_shape': True,
            'in_ports_count': 2,
            'out_ports_count': 1,
            'infer': self.infer
        }, attrs)

    @staticmethod
    def infer(node):
        if len(node.in_nodes()) <= 1:
            raise Error('There is no input with unsqueeze dims for the node {}'.format(node.soft_get('name')))
        unsqueeze_dims = node.in_port(1).data.get_value()
        if unsqueeze_dims is None:
            raise Error('The dimensions to unsqueeze are not defined for the node {}'.format(node.soft_get('name')))
        unsqueeze_dims = int64_array(unsqueeze_dims)

        input_value = node.in_port(0).data.get_value()
        input_shape = node.in_port(0).data.get_shape()

        # TODO remove the following line when the Inference Engine plugins support 0D tensors
        if unsqueeze_dims.ndim == 0:
            unsqueeze_dims = int64_array([unsqueeze_dims.item()])

        # make dimensions positive to correctly translate from NHWC to NCHW layout
        unsqueeze_dims = int64_array([dim + len(node.in_port(0).data.get_shape()) + 1 if dim < 0 else dim
                                      for dim in unsqueeze_dims])
        if node.in_port(1).get_source().node.op == 'Const':
            node.in_port(1).data.set_value(unsqueeze_dims)

        output_shape = input_shape.copy()
        for dim in unsqueeze_dims:
            output_shape = shape_insert(output_shape, dim, 1)

        if input_value is not None and is_fully_defined(output_shape):
            node.out_port(0).data.set_value(input_value.reshape(output_shape))
        else:
            node.out_port(0).data.set_shape(output_shape)

        PermuteInputs().set_input_permutation(node.in_node(1), node, 'input:0', 'axis')
