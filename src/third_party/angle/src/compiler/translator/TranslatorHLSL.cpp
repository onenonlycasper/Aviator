//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorHLSL.h"

#include "compiler/translator/InitializeParseContext.h"
#include "compiler/translator/OutputHLSL.h"

TranslatorHLSL::TranslatorHLSL(ShShaderType type, ShShaderSpec spec, ShShaderOutput output)
  : TCompiler(type, spec, output)
{
}

void TranslatorHLSL::translate(TIntermNode *root)
{
    TParseContext& parseContext = *GetGlobalParseContext();
    sh::OutputHLSL outputHLSL(parseContext, getResources(), getOutputType());

    outputHLSL.output();

    mActiveUniforms         = outputHLSL.getUniforms();
    mActiveInterfaceBlocks  = outputHLSL.getInterfaceBlocks();
    mActiveOutputVariables  = outputHLSL.getOutputVariables();
    mActiveAttributes       = outputHLSL.getAttributes();
    mActiveVaryings         = outputHLSL.getVaryings();
}
