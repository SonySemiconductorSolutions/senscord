/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util/resource_list.h"

#include <map>
#include <utility>

#include "util/mutex.h"
#include "util/autolock.h"

namespace senscord {

struct ResourceList::Impl {
  util::Mutex mutex;
  std::map<std::string, ResourceData*> list;
};

/**
 * @brief Constructor.
 */
ResourceList::ResourceList() : pimpl_(new Impl()) {
}

/**
 * @brief Destructor.
 *
 * Releases the registered resource data.
 */
ResourceList::~ResourceList() {
  {
    util::AutoLock _lock(&pimpl_->mutex);
    for (std::map<std::string, ResourceData*>::const_iterator
        itr = pimpl_->list.begin(), end = pimpl_->list.end();
        itr != end; ++itr) {
      delete itr->second;
    }
  }
  delete pimpl_;
}

/**
 * @brief Creates a resource data.
 * @param[in] (key) Resource key.
 * @param[in] (factory) Resource data factory.
 * @return Resource data.
 */
ResourceData* ResourceList::CreateImpl(
    const std::string& key, const Factory& factory) {
  util::AutoLock _lock(&pimpl_->mutex);
  std::pair<std::map<std::string, ResourceData*>::iterator, bool> ret =
      pimpl_->list.insert(std::make_pair(
          key, reinterpret_cast<ResourceData*>(NULL)));
  if (ret.second) {
    ret.first->second = factory.Create();
  }
  return ret.first->second;
}

/**
 * @brief Gets a resource data.
 * @param[in] (key) Resource key.
 * @return Resource data. (NULL if no id exists)
 */
ResourceData* ResourceList::GetImpl(const std::string& key) const {
  ResourceData* data = NULL;
  util::AutoLock _lock(&pimpl_->mutex);
  std::map<std::string, ResourceData*>::const_iterator itr =
      pimpl_->list.find(key);
  if (itr != pimpl_->list.end()) {
    data = itr->second;
  }
  return data;
}

/**
 * @brief Releases a resource data.
 * @param[in] (key) Resource key.
 */
Status ResourceList::Release(const std::string& key) {
  util::AutoLock _lock(&pimpl_->mutex);
  std::map<std::string, ResourceData*>::iterator itr = pimpl_->list.find(key);
  if (itr == pimpl_->list.end()) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseNotFound,
        "data not found: key=%s", key.c_str());
  }
  delete itr->second;
  pimpl_->list.erase(itr);
  return Status::OK();
}

}  // namespace senscord
