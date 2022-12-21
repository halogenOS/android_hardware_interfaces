/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <aidl/Vintf.h>

#define LOG_TAG "VtsHalNSParamTest"

#include <Utils.h>
#include "EffectHelper.h"

using namespace android;

using aidl::android::hardware::audio::effect::Capability;
using aidl::android::hardware::audio::effect::Descriptor;
using aidl::android::hardware::audio::effect::IEffect;
using aidl::android::hardware::audio::effect::IFactory;
using aidl::android::hardware::audio::effect::kNoiseSuppressionTypeUUID;
using aidl::android::hardware::audio::effect::NoiseSuppression;
using aidl::android::hardware::audio::effect::Parameter;

enum ParamName { PARAM_INSTANCE_NAME, PARAM_LEVEL };
using NSParamTestParam =
        std::tuple<std::pair<std::shared_ptr<IFactory>, Descriptor>, NoiseSuppression::Level>;

class NSParamTest : public ::testing::TestWithParam<NSParamTestParam>, public EffectHelper {
  public:
    NSParamTest() : mLevel(std::get<PARAM_LEVEL>(GetParam())) {
        std::tie(mFactory, mDescriptor) = std::get<PARAM_INSTANCE_NAME>(GetParam());
    }

    void SetUp() override {
        ASSERT_NE(nullptr, mFactory);
        ASSERT_NO_FATAL_FAILURE(create(mFactory, mEffect, mDescriptor));

        Parameter::Specific specific = getDefaultParamSpecific();
        Parameter::Common common = EffectHelper::createParamCommon(
                0 /* session */, 1 /* ioHandle */, 44100 /* iSampleRate */, 44100 /* oSampleRate */,
                kInputFrameCount /* iFrameCount */, kOutputFrameCount /* oFrameCount */);
        IEffect::OpenEffectReturn ret;
        ASSERT_NO_FATAL_FAILURE(open(mEffect, common, specific, &ret, EX_NONE));
        ASSERT_NE(nullptr, mEffect);
    }

    void TearDown() override {
        ASSERT_NO_FATAL_FAILURE(close(mEffect));
        ASSERT_NO_FATAL_FAILURE(destroy(mFactory, mEffect));
    }

    Parameter::Specific getDefaultParamSpecific() {
        NoiseSuppression ns =
                NoiseSuppression::make<NoiseSuppression::level>(NoiseSuppression::Level::MEDIUM);
        Parameter::Specific specific =
                Parameter::Specific::make<Parameter::Specific::noiseSuppression>(ns);
        return specific;
    }

    static const long kInputFrameCount = 0x100, kOutputFrameCount = 0x100;
    static const std::vector<std::pair<std::shared_ptr<IFactory>, Descriptor>> kFactoryDescList;
    static const std::vector<NoiseSuppression::Level> kLevelValues;

    std::shared_ptr<IFactory> mFactory;
    std::shared_ptr<IEffect> mEffect;
    Descriptor mDescriptor;
    NoiseSuppression::Level mLevel;

    void SetAndGetParameters() {
        for (auto& it : mTags) {
            auto& tag = it.first;
            auto& ns = it.second;

            // validate parameter
            Descriptor desc;
            ASSERT_STATUS(EX_NONE, mEffect->getDescriptor(&desc));
            const binder_exception_t expected = EX_NONE;

            // set parameter
            Parameter expectParam;
            Parameter::Specific specific;
            specific.set<Parameter::Specific::noiseSuppression>(ns);
            expectParam.set<Parameter::specific>(specific);
            EXPECT_STATUS(expected, mEffect->setParameter(expectParam)) << expectParam.toString();

            // only get if parameter in range and set success
            if (expected == EX_NONE) {
                Parameter getParam;
                Parameter::Id id;
                NoiseSuppression::Id specificId;
                specificId.set<NoiseSuppression::Id::commonTag>(tag);
                id.set<Parameter::Id::noiseSuppressionTag>(specificId);
                EXPECT_STATUS(EX_NONE, mEffect->getParameter(id, &getParam));

                EXPECT_EQ(expectParam, getParam) << "\nexpect:" << expectParam.toString()
                                                 << "\ngetParam:" << getParam.toString();
            }
        }
    }

    void addLevelParam(NoiseSuppression::Level level) {
        NoiseSuppression ns;
        ns.set<NoiseSuppression::level>(level);
        mTags.push_back({NoiseSuppression::level, ns});
    }

  private:
    std::vector<std::pair<NoiseSuppression::Tag, NoiseSuppression>> mTags;
    void CleanUp() { mTags.clear(); }
};

const std::vector<std::pair<std::shared_ptr<IFactory>, Descriptor>> kFactoryDescList =
        EffectFactoryHelper::getAllEffectDescriptors(IFactory::descriptor,
                                                     kNoiseSuppressionTypeUUID);
const std::vector<NoiseSuppression::Level> NSParamTest::kLevelValues = {
        NoiseSuppression::Level::LOW, NoiseSuppression::Level::MEDIUM,
        NoiseSuppression::Level::HIGH};

TEST_P(NSParamTest, SetAndGetLevel) {
    EXPECT_NO_FATAL_FAILURE(addLevelParam(mLevel));
    SetAndGetParameters();
}

INSTANTIATE_TEST_SUITE_P(
        NSParamTest, NSParamTest,
        ::testing::Combine(testing::ValuesIn(EffectFactoryHelper::getAllEffectDescriptors(
                                   IFactory::descriptor, kNoiseSuppressionTypeUUID)),
                           testing::ValuesIn(NSParamTest::kLevelValues)),
        [](const testing::TestParamInfo<NSParamTest::ParamType>& info) {
            auto descriptor = std::get<PARAM_INSTANCE_NAME>(info.param).second;
            std::string level = aidl::android::hardware::audio::effect::toString(
                    std::get<PARAM_LEVEL>(info.param));
            std::string name = "Implementor_" + descriptor.common.implementor + "_name_" +
                               descriptor.common.name + "_UUID_" +
                               descriptor.common.id.uuid.toString() + "_level_" + level;
            std::replace_if(
                    name.begin(), name.end(), [](const char c) { return !std::isalnum(c); }, '_');
            return name;
        });

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NSParamTest);

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();
    return RUN_ALL_TESTS();
}