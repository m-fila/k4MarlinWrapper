/*
 * Copyright (c) 2019-2024 Key4hep-Project.
 *
 * This file is part of Key4hep.
 * See https://key4hep.github.io/key4hep-doc/ for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <GaudiKernel/IEventProcessor.h>

#include "k4MarlinWrapper/LCEventWrapper.h"
#include "k4MarlinWrapper/LcioEventAlgo.h"

#include "marlin/Global.h"
#include "marlin/StringParameters.h"

#include <EVENT/LCIO.h>
#include <MT/LCReader.h>

#include <memory>
#include <string>

DECLARE_COMPONENT(LcioEvent)

LcioEvent::LcioEvent(const std::string& name, ISvcLocator* pSL) : Gaudi::Algorithm(name, pSL) {}

StatusCode LcioEvent::initialize() {
  StatusCode sc = Gaudi::Algorithm::initialize();
  if (sc.isFailure())
    return sc;

  marlin::Global::parameters->add("LCIOInputFiles", m_fileNames);

  m_reader = new MT::LCReader(0);
  m_reader->open(m_fileNames);
  info() << "Initialized the LcioEvent Algo: " << m_fileNames[0] << endmsg;
  return StatusCode::SUCCESS;
}

StatusCode LcioEvent::execute(const EventContext&) const {
  auto theEvent = m_reader->readNextEvent(EVENT::LCIO::UPDATE);

  if (!theEvent) {
    // Store flag to indicate there was NOT a LCEvent
    auto             pStatus  = std::make_unique<LCEventWrapperStatus>(false);
    const StatusCode scStatus = eventSvc()->registerObject("/Event/LCEventStatus", pStatus.release());
    if (scStatus.isFailure()) {
      error() << "Failed to store flag for underlying LCEvent: MarlinProcessorWrapper may try to run over non existing "
                 "event"
              << endmsg;
      return scStatus;
    }

    auto svc = service<IEventProcessor>("ApplicationMgr");
    if (svc) {
      svc->stopRun().ignore();
      svc->release();
    } else {
      abort();
    }
  } else {
    // pass theEvent to the DataStore, so we can access them in our processor
    // wrappers
    info() << "Reading from file: " << m_fileNames[0] << endmsg;

    auto             myEvWr = new LCEventWrapper(std::move(theEvent));
    const StatusCode sc     = eventSvc()->registerObject("/Event/LCEvent", myEvWr);
    if (sc.isFailure()) {
      error() << "Failed to store the LCEvent" << endmsg;
      return sc;
    } else {
      // Store flag to indicate there was a LCEvent
      auto pStatus = std::make_unique<LCEventWrapperStatus>(true);
      std::cout << "Saving status: " << pStatus->hasLCEvent << std::endl;
      const StatusCode scStatus = eventSvc()->registerObject("/Event/LCEventStatus", pStatus.release());
      if (scStatus.isFailure()) {
        error() << "Failed to store flag for underlying LCEvent: MarlinProcessorWrapper may try to run over non "
                   "existing event"
                << endmsg;
        return scStatus;
      }
    }
  }

  return StatusCode::SUCCESS;
}
