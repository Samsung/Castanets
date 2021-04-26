// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace {
constexpr const char kNearbySharingFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const uint8_t kNearbySharingFastPairId[] = {0xfc, 0x12, 0x8e};
}  // namespace

// static
FastInitiationManager::Factory*
    FastInitiationManager::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<FastInitiationManager> FastInitiationManager::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(adapter);

  return std::make_unique<FastInitiationManager>(adapter);
}

// static
void FastInitiationManager::Factory::SetFactoryForTesting(
    FastInitiationManager::Factory* factory) {
  factory_instance_ = factory;
}

FastInitiationManager::FastInitiationManager(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter && adapter->IsPresent() && adapter->IsPowered());
  adapter_ = adapter;
}

FastInitiationManager::~FastInitiationManager() {
  StopAdvertising(base::DoNothing());
}

void FastInitiationManager::StartAdvertising(
    base::OnceCallback<void()> callback,
    base::OnceCallback<void()> error_callback) {
  DCHECK(adapter_->IsPresent() && adapter_->IsPowered());
  DCHECK(!advertisement_);

  // These callbacks are instances of OnceCallback, but RegisterAdvertisement()
  // expects RepeatingCallback. Passing these as arguments is possible using
  // Passed(), but this is dangerous so we just store them.
  start_callback_ = std::move(callback);
  start_error_callback_ = std::move(error_callback);

  // TODO(hansenmichael): Lower Bluetooth advertising interval to 100ms for
  // faster discovery. Be sure to restore the interval when we stop
  // broadcasting.

  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  auto list = std::make_unique<device::BluetoothAdvertisement::UUIDList>();
  list->push_back(kNearbySharingFastInitiationServiceUuid);
  advertisement_data->set_service_uuids(std::move(list));

  auto service_data =
      std::make_unique<device::BluetoothAdvertisement::ServiceData>();
  auto payload = std::vector<uint8_t>(std::begin(kNearbySharingFastPairId),
                                      std::end(kNearbySharingFastPairId));
  payload.push_back(GenerateFastInitV1Metadata());
  service_data->insert(std::pair<std::string, std::vector<uint8_t>>(
      kNearbySharingFastInitiationServiceUuid, payload));
  advertisement_data->set_service_data(std::move(service_data));

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::Bind(&FastInitiationManager::OnAdvertisementRegistered,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&FastInitiationManager::OnErrorRegisteringAdvertisement,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FastInitiationManager::StopAdvertising(
    base::OnceCallback<void()> callback) {
  stop_callback_ = std::move(callback);

  if (!advertisement_) {
    std::move(stop_callback_).Run();
    return;
  }

  advertisement_->RemoveObserver(this);
  advertisement_->Unregister(
      base::Bind(&FastInitiationManager::OnAdvertisementUnregistered,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&FastInitiationManager::OnErrorUnregisteringAdvertisement,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FastInitiationManager::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  // TODO(hansenmichael): Handle advertisement released appropriately.
}

void FastInitiationManager::OnAdvertisementRegistered(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  advertisement_ = advertisement;
  advertisement_->AddObserver(this);
  std::move(start_callback_).Run();
  start_error_callback_.Reset();
}

void FastInitiationManager::OnErrorRegisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  LOG(ERROR)
      << "FastInitiationManager::StartAdvertising() failed with error code = "
      << error_code;
  std::move(start_error_callback_).Run();
  start_callback_.Reset();
}

void FastInitiationManager::OnAdvertisementUnregistered() {
  advertisement_.reset();
  std::move(stop_callback_).Run();
}

void FastInitiationManager::OnErrorUnregisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  LOG(WARNING)
      << "FastInitiationManager::StopAdvertising() failed with error code = "
      << error_code;
  advertisement_.reset();
  stop_callback_.Reset();
}

uint8_t FastInitiationManager::GenerateFastInitV1Metadata() {
  // TODO(hansenmichael): Include 'version', |type|, and |adjusted_tx_power|
  // bits.
  return 0x00;
}
