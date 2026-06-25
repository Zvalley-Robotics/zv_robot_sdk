#pragma once

#include <dds/dds.hpp>

using namespace org::eclipse::cyclonedds;

namespace zv::common
{

/*
 * DdsParticipantQos
 */
using DdsParticipantQos = dds::domain::qos::DomainParticipantQos;

/*
 * DdsTopicQos
 */
using DdsTopicQos = dds::topic::qos::TopicQos;

/*
 * DdsPublisherQos
 */
using DdsPublisherQos = dds::pub::qos::PublisherQos;

/*
 * DdsSubscriberQos
 */
using DdsSubscriberQos = dds::sub::qos::SubscriberQos;

/*
 * DdsWriterQos
 */
using DdsWriterQos = dds::pub::qos::DataWriterQos;

/*
 * DdsReaderQos
 */
using DdsReaderQos = dds::sub::qos::DataReaderQos;

}
