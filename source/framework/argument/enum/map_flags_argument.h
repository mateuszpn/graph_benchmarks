/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "framework/argument/abstract/enum_argument.h"
#include "framework/enum/map_flags.h"

struct MapFlagsArgument : EnumArgument<MapFlagsArgument, MapFlags> {
  using EnumArgument::EnumArgument;
  ThisType &operator=(EnumType newValue) {
    this->value = newValue;
    markAsParsed();
    return *this;
  }

  const static inline std::string enumName = "map flag";
  const static inline EnumType invalidEnumValue = EnumType::Unknown;
  const static inline EnumType enumValues[3] = {EnumType::Read, EnumType::Write,
                                                EnumType::WriteInvalidate};
  const static inline std::string enumValuesNames[3] = {"Read", "Write",
                                                        "WriteInvalidate"};
};
