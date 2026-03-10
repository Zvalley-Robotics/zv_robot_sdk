#pragma once

#include <dds/dds.hpp>

#include <zv/common/dds/dds_callback.hpp>

/*
 * dds wait sub/pub matched default time.
 * default 10000 us
 */
#define ZV_DDS_WAIT_MATCHED_TIME_MIN   10000
#define ZV_DDS_WAIT_MATCHED_TIME_MAX   1000000

namespace zv::common
{

using namespace org::eclipse::cyclonedds;

template<typename T>
T& getRef(const std::shared_ptr<T>& ptr) {
    if (!ptr) {
        throw std::runtime_error("Dereferencing null shared_ptr");
    }
    return *ptr;
}

// class DdsParticipant {
// public:
//     using NATIVE_TYPE = dds::domain::DomainParticipant;

// }

// class DdsReaderListener {
// public:
//     explicit

// };

using DdsParticipantPtr = std::shared_ptr<dds::domain::DomainParticipant>;
using DdsPublisherPtr = std::shared_ptr<dds::pub::Publisher>;
using DdsSubscriberPtr = std::shared_ptr<dds::sub::Subscriber>;

template<typename MSG>
using DdsTopic = dds::topic::Topic<MSG>;

template<typename MSG>
using DdsTopicPtr = std::shared_ptr<dds::topic::Topic<MSG>>;

template<typename MSG>
using DdsWriter = dds::pub::DataWriter<MSG>;

template<typename MSG>
using DdsWriterPtr = std::shared_ptr<dds::pub::DataWriter<MSG>>;

template<typename MSG>
using DdsReader = dds::sub::DataReader<MSG>;

template<typename MSG>
using DdsReaderPtr = std::shared_ptr<dds::sub::DataReader<MSG>>;

// template<typename MSG>
// using DdsReaderListener = dds::sub::NoOpDataReaderListener<MSG>;

/*
 * @brief: DdsReaderListener
 */
template<typename MSG>
class DdsReaderListener : public virtual dds::sub::NoOpDataReaderListener<MSG>
{
public:
    using NATIVE_TYPE = ::dds::sub::DataReaderListener<MSG>;
    using MSG_PTR = std::shared_ptr<MSG>;

    explicit DdsReaderListener() :
        mMask(::dds::core::status::StatusMask::none())
    {}

    ~DdsReaderListener()
    {}

    void SetCallback(const DdsReaderCallback& cb)
    {
        if (cb.HasMessageHandler())
        {
            mMask |= ::dds::core::status::StatusMask::data_available();
        }

        mCallbackPtr.reset(new DdsReaderCallback(cb));
    }

    const ::dds::core::status::StatusMask& GetStatusMask() const
    {
        return mMask;
    }

private:
    void on_data_available(::dds::sub::DataReader<MSG>& reader)
    {
        ::dds::sub::LoanedSamples<MSG> samples;
        samples = reader.take();

        if (samples.length() <= 0)
        {
            return;
        }

        typename ::dds::sub::LoanedSamples<MSG>::const_iterator iter;
        for (iter=samples.begin(); iter<samples.end(); ++iter)
        {
            const MSG& m = iter->data();
            if (iter->info().valid())
            {
                mCallbackPtr->OnDataAvailable((const void*)&m);
            }
        }
    }

private:
    dds::core::status::StatusMask mMask;

    DdsReaderCallbackPtr mCallbackPtr;
};

template<typename MSG>
using DdsReaderListenerPtr = std::shared_ptr<DdsReaderListener<MSG>>;




// template <typename MSG>
// class DdsReaderListener : public DataReaderListener {
// public:
//     DdsReaderListener() = default;
//     ~DdsReaderListener() override = default;

//     void SetCallback(const DdsReaderCallback &cb) {
//         if (!cb.HasMessageHandler()) {
//             std::cerr << "Listener Set Callback: invalid hanlder" << std::endl;
//             return;
//         }
//         cb_ = std::make_shared<DdsReaderCallback>(cb);
//     }

//     void on_data_available(DataReader *reader) override {
//         MSG st;
//         SampleInfo info;
//         if (reader->take_next_sample(&st, &info) == ReturnCode_t::RETCODE_OK) {
//             if (info.valid_data) {
//                 cb_->OnDataAvailable(&st);
//             }
//         }
//     }

// private:
//     DdsReaderCallbackPtr cb_;
// };

// template <typename MSG>
// using DdsReaderListenerPtr = std::shared_ptr<DdsReaderListener<MSG>>;
}