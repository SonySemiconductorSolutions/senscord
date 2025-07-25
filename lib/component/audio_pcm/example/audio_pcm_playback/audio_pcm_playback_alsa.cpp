/*
 * SPDX-FileCopyrightText: 2023 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio_pcm_playback_alsa.h"

#include <string>
#include <vector>

#include <alsa/asoundlib.h>

namespace {

const char kBlockName[] = "audio";

const uint32_t kDefaultChannels = 2;
const bool kDefaultInterleaved = true;
const senscord::AudioPcm::Format kDefaultFormat =
    senscord::AudioPcm::kFormatS16LE;
const uint32_t kDefaultSampleRate = 44100;

struct FormatInfo {
  senscord::AudioPcm::Format format;
  snd_pcm_format_t alsa_format;
};

const FormatInfo kFormatTable[] = {
    // 8bit
    { senscord::AudioPcm::kFormatS8, SND_PCM_FORMAT_S8 },
    { senscord::AudioPcm::kFormatU8, SND_PCM_FORMAT_U8 },
    // 16bit
    { senscord::AudioPcm::kFormatS16LE, SND_PCM_FORMAT_S16_LE },
    { senscord::AudioPcm::kFormatS16BE, SND_PCM_FORMAT_S16_BE },
    { senscord::AudioPcm::kFormatU16LE, SND_PCM_FORMAT_U16_LE },
    { senscord::AudioPcm::kFormatU16BE, SND_PCM_FORMAT_U16_BE },
    // 24bit
    { senscord::AudioPcm::kFormatS24LE3, SND_PCM_FORMAT_S24_3LE },
    { senscord::AudioPcm::kFormatS24BE3, SND_PCM_FORMAT_S24_3BE },
    { senscord::AudioPcm::kFormatU24LE3, SND_PCM_FORMAT_U24_3LE },
    { senscord::AudioPcm::kFormatU24BE3, SND_PCM_FORMAT_U24_3BE },
    // 24bit (using lower 3 bytes out of 4 bytes)
    { senscord::AudioPcm::kFormatS24LE, SND_PCM_FORMAT_S24_LE },
    { senscord::AudioPcm::kFormatS24BE, SND_PCM_FORMAT_S24_BE },
    { senscord::AudioPcm::kFormatU24LE, SND_PCM_FORMAT_U24_LE },
    { senscord::AudioPcm::kFormatU24BE, SND_PCM_FORMAT_U24_BE },
    // 32bit
    { senscord::AudioPcm::kFormatS32LE, SND_PCM_FORMAT_S32_LE },
    { senscord::AudioPcm::kFormatS32BE, SND_PCM_FORMAT_S32_BE },
    { senscord::AudioPcm::kFormatU32LE, SND_PCM_FORMAT_U32_LE },
    { senscord::AudioPcm::kFormatU32BE, SND_PCM_FORMAT_U32_BE },
    // 32bit float
    { senscord::AudioPcm::kFormatFloat32LE, SND_PCM_FORMAT_FLOAT_LE },
    { senscord::AudioPcm::kFormatFloat32BE, SND_PCM_FORMAT_FLOAT_BE },
    // 64bit float
    { senscord::AudioPcm::kFormatFloat64LE, SND_PCM_FORMAT_FLOAT64_LE },
    { senscord::AudioPcm::kFormatFloat64BE, SND_PCM_FORMAT_FLOAT64_BE },
};

/**
 * @brief Get ALSA format from `AudioPcm::Format`.
 */
snd_pcm_format_t GetAlsaFormat(senscord::AudioPcm::Format format) {
  size_t count = sizeof(kFormatTable) / sizeof(kFormatTable[0]);
  for (size_t i = 0; i < count; ++i) {
    if (kFormatTable[i].format == format) {
      return kFormatTable[i].alsa_format;
    }
  }
  return SND_PCM_FORMAT_UNKNOWN;
}

}  // namespace

struct AudioPcmPlaybackAlsa::Impl {
  snd_pcm_t* playback_handle;
  senscord::AudioPcmProperty params;
  bool running;
};

AudioPcmPlaybackAlsa::AudioPcmPlaybackAlsa() : pimpl_(new Impl) {
  pimpl_->params.channels = kDefaultChannels;
  pimpl_->params.interleaved = kDefaultInterleaved;
  pimpl_->params.format = kDefaultFormat;
  pimpl_->params.samples_per_second = kDefaultSampleRate;
  pimpl_->params.samples_per_frame = 0;
  pimpl_->running = false;
}

AudioPcmPlaybackAlsa::~AudioPcmPlaybackAlsa() {
  delete pimpl_;
}

senscord::Status AudioPcmPlaybackAlsa::Open(const std::string& device_name) {
  int32_t err = 0;
  err = snd_pcm_open(
      &pimpl_->playback_handle, device_name.c_str(),
      SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    return SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseHardwareError,
        "Failed to snd_pcm_open: device_name=%s (%s)",
        device_name.c_str(), snd_strerror(err));
  }
  return senscord::Status::OK();
}

senscord::Status AudioPcmPlaybackAlsa::Close() {
  if (pimpl_->playback_handle != NULL) {
    snd_pcm_close(pimpl_->playback_handle);
    pimpl_->playback_handle = NULL;
  }
  pimpl_->running = false;
  return senscord::Status::OK();
}

senscord::Status AudioPcmPlaybackAlsa::SetParams(
    const senscord::AudioPcmProperty& params) {
  senscord::Status status;
  snd_pcm_hw_params_t* hw_params = NULL;

  if (status.ok()) {
    int32_t err = snd_pcm_hw_params_malloc(&hw_params);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params_malloc (%s)",
          snd_strerror(err));
    }
  }

  if (status.ok()) {
    int32_t err = snd_pcm_hw_params_any(pimpl_->playback_handle, hw_params);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params_any (%s)",
          snd_strerror(err));
    }
  }

  // set access type (interleaved / non-interleaved)
  if (status.ok()) {
    snd_pcm_access_t accesss = params.interleaved ?
        SND_PCM_ACCESS_RW_INTERLEAVED : SND_PCM_ACCESS_RW_NONINTERLEAVED;
    int32_t err = snd_pcm_hw_params_set_access(
        pimpl_->playback_handle, hw_params, accesss);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params_set_access (%s)",
          snd_strerror(err));
    }
  }

  // set sample format
  if (status.ok()) {
    snd_pcm_format_t format = GetAlsaFormat(params.format);
    int32_t err = snd_pcm_hw_params_set_format(
        pimpl_->playback_handle, hw_params, format);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params_set_format: format=%d (%s)",
          format, snd_strerror(err));
    }
  }

  // set sample rate
  uint32_t rate = params.samples_per_second;
  if (status.ok()) {
    int32_t err = snd_pcm_hw_params_set_rate_near(
        pimpl_->playback_handle, hw_params, &rate, NULL);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params_set_rate_near: rate=%" PRIu32 " (%s)",
          rate, snd_strerror(err));
    }
  }

  // set channel count
  if (status.ok()) {
    int32_t err = snd_pcm_hw_params_set_channels(
        pimpl_->playback_handle, hw_params, params.channels);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params_set_channels: channels=%" PRIu8 " (%s)",
          params.channels, snd_strerror(err));
    }
  }

  // TODO: set period size, buffer size

  if (status.ok()) {
    int32_t err = snd_pcm_hw_params(pimpl_->playback_handle, hw_params);
    if (err < 0) {
      status = SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_hw_params (%s)",
          snd_strerror(err));
    }
  }

  if (hw_params != NULL) {
    snd_pcm_hw_params_free(hw_params);
  }

  if (status.ok()) {
    pimpl_->params = params;
    pimpl_->params.samples_per_second = rate;
  }

  return status;
}

senscord::Status AudioPcmPlaybackAlsa::GetParams(
    senscord::AudioPcmProperty* params) {
  *params = pimpl_->params;
  return senscord::Status::OK();
}

senscord::Status AudioPcmPlaybackAlsa::Start() {
  if (!pimpl_->running) {
    int32_t err = snd_pcm_start(pimpl_->playback_handle);
    if (err < 0) {
      return SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_start (%s)",
          snd_strerror(err));
    }
    pimpl_->running = true;
  }
  return senscord::Status::OK();
}

senscord::Status AudioPcmPlaybackAlsa::Stop() {
  if (pimpl_->running) {
    int32_t err = snd_pcm_drain(pimpl_->playback_handle);
    if (err < 0) {
      return SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseHardwareError,
          "Failed to snd_pcm_drain (%s)",
          snd_strerror(err));
    }
    pimpl_->running = false;
  }
  return senscord::Status::OK();
}

senscord::Status AudioPcmPlaybackAlsa::Write(senscord::Frame* frame) {
  if (!pimpl_->running) {
    return senscord::Status::OK();
  }
  senscord::ChannelList list;
  frame->GetChannelList(&list);
  if (list.empty()) {
    return SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseNotFound,
        "ChannelList empty");
  }
  snd_pcm_sframes_t write_count = 0;
  snd_pcm_sframes_t input_count = 0;
  if (pimpl_->params.interleaved) {
    senscord::Channel* channel = list.begin()->second;
    senscord::Channel::RawData rawdata = {};
    channel->GetRawData(&rawdata);
    input_count = rawdata.size / (pimpl_->params.channels *
        senscord::AudioPcm::GetByteWidth(pimpl_->params.format));
    write_count = snd_pcm_writei(
        pimpl_->playback_handle, rawdata.address, input_count);
    if (write_count < 0) {
      snd_pcm_recover(pimpl_->playback_handle, write_count, 0);
    }
  } else {
    std::vector<void*> buf;
    for (senscord::ChannelList::const_iterator itr = list.begin(),
        end = list.end(); itr != end; ++itr) {
      senscord::Channel* channel = itr->second;
      senscord::Channel::RawData rawdata = {};
      channel->GetRawData(&rawdata);
      input_count = rawdata.size /
          senscord::AudioPcm::GetByteWidth(pimpl_->params.format);
      buf.push_back(rawdata.address);
    }
    write_count = snd_pcm_writen(
        pimpl_->playback_handle, &buf[0], input_count);
  }
  if (write_count != input_count) {
    return SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseHardwareError,
        "Failed to snd_pcm_write: interleaved=%d, expected=%ld, write=%ld",
        pimpl_->params.interleaved, input_count, write_count);
  }

  return senscord::Status::OK();
}
