/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_CORE_UTIL_RESOURCE_LIST_H_
#define LIB_CORE_UTIL_RESOURCE_LIST_H_

#include <string>

#include "senscord/noncopyable.h"
#include "senscord/status.h"

namespace senscord {

/**
 * @brief Interface for resource data.
 *
 * Inheriting classes cannot use constructors with arguments.
 */
struct ResourceData : private util::Noncopyable {
  virtual ~ResourceData() {}
};

/**
 * @brief List of resource data.
 */
class ResourceList {
 public:
  /**
   * @brief Constructor.
   */
  ResourceList();

  /**
   * @brief Destructor.
   *
   * Releases the registered resource data.
   */
  ~ResourceList();

  /**
   * @brief Creates a resource data.
   * @param[in] (key) Resource key.
   * @return Resource data.
   */
  template<typename DerivedResourceData>
  DerivedResourceData* Create(const std::string& key) {
    FactoryDerived<DerivedResourceData> factory;
    return static_cast<DerivedResourceData*>(CreateImpl(key, factory));
  }

  /**
   * @brief Gets a resource data.
   * @param[in] (key) Resource key.
   * @return Resource data. (NULL if no id exists)
   */
  template<typename DerivedResourceData>
  DerivedResourceData* Get(const std::string& key) const {
    return static_cast<DerivedResourceData*>(GetImpl(key));
  }

  /**
   * @brief Releases a resource data.
   * @param[in] (key) Resource key.
   * @return Status object.
   */
  Status Release(const std::string& key);

 private:
  /**
   * @brief A factory that creates an instance of a class.
   */
  struct Factory {
    virtual ~Factory() {}
    virtual ResourceData* Create() const = 0;
  };
  template<typename Derived>
  struct FactoryDerived : public Factory {
    virtual ResourceData* Create() const { return new Derived(); }
  };

  /**
   * @brief Creates a resource data.
   * @param[in] (key) Resource key.
   * @param[in] (factory) Resource data factory.
   * @return Resource data.
   */
  ResourceData* CreateImpl(const std::string& key, const Factory& factory);

  /**
   * @brief Gets a resource data.
   * @param[in] (key) Resource key.
   * @return Resource data. (NULL if no id exists)
   */
  ResourceData* GetImpl(const std::string& key) const;

 private:
  struct Impl;
  Impl* pimpl_;
};

}  // namespace senscord

#endif  // LIB_CORE_UTIL_RESOURCE_LIST_H_
