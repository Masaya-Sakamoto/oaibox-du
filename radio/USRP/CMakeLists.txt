if (OAI_TMYTEK_UD_BBOX OR OAI_TMYTEK_AIP)
  find_package(Boost REQUIRED COMPONENTS program_options system thread)
  find_package(UHD REQUIRED)
  # we need pybind for TMYTEK libraries
  find_package(pybind11 REQUIRED)
  # or add_subdirectory(pybind11)
  # or cmake -Dpybind11_DIR={your path for pybind11xxx.cmake} ..

  # Add USRP SPI over GPIO library
  add_library(usrp_fbs SHARED ${CMAKE_CURRENT_SOURCE_DIR}/lib_usrp_spi/src/usrp_fbs.cpp)
  target_include_directories(usrp_fbs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib_usrp_spi/include)
  target_link_libraries(usrp_fbs PRIVATE ${UHD_LIBRARIES} ${Boost_LIBRARIES})
  set_target_properties(usrp_fbs PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  # suppress compiler warnings
  target_compile_options(usrp_fbs PUBLIC -Wno-return-type)

  # Add TMYTEK TLKCORE library
  add_library(tlkcore_lib SHARED ${CMAKE_CURRENT_SOURCE_DIR}/lib_tlkcore_cpp/src/tlkcore_lib.cpp)
  target_include_directories(tlkcore_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib_tlkcore_cpp/include)
  target_link_libraries(tlkcore_lib PRIVATE pybind11::embed)
  set_target_properties(tlkcore_lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  # suppress compiler warnings
  target_compile_options(tlkcore_lib PUBLIC -Wno-attributes -Wno-comment)

  # LIB
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E copy_directory # copy_directory | create_symlink
          ${CMAKE_SOURCE_DIR}/radio/USRP/lib
          ${CMAKE_BINARY_DIR}/lib
  )
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E copy_directory # copy_directory | create_symlink
          ${CMAKE_SOURCE_DIR}/radio/USRP/lib_tlkcore_cpp/lib
          ${CMAKE_BINARY_DIR}/lib
  )

  # CACHE
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_BINARY_DIR}/lib/tlkcore/__pycache__
  )
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_BINARY_DIR}/lib/__pycache__
  )

  if (OAI_TMYTEK_UD_BBOX)
    # Calibration and configuration files
    file(GLOB CALIB_LIST
            RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/setup setup/*.zip
    )
    if (NOT CALIB_LIST)
      message(FATAL_ERROR "Calibration files in radio/USRP/setup are missing, please provide your tables from TMYTEK in a zip file and configure device.conf!")
    endif ()
  endif ()

  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_BINARY_DIR}/files/BeamTable
  )
  if (OAI_TMYTEK_UD_BBOX)
    FOREACH (CALIB ${CALIB_LIST})
      file(
              ARCHIVE_EXTRACT
              INPUT ${CMAKE_SOURCE_DIR}/radio/USRP/setup/${CALIB}
              DESTINATION ${CMAKE_BINARY_DIR}/files
      )
    ENDFOREACH ()
  endif ()

  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_BINARY_DIR}/config
  )
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/radio/USRP/setup/device.conf
          ${CMAKE_BINARY_DIR}/config/device.conf
  )
  if (OAI_TMYTEK_UD_BBOX)
    list(TRANSFORM CALIB_LIST REPLACE "[.]zip$" ".csv")
    FOREACH (CALIB ${CALIB_LIST})
      add_custom_command(
              TARGET tlkcore_lib
              PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
              COMMAND
              ${CMAKE_COMMAND} -E create_symlink
              ${CMAKE_SOURCE_DIR}/radio/USRP/setup/CustomBatchBeams.csv
              ${CMAKE_BINARY_DIR}/config/CustomBatchBeams_${CALIB}
      )
    ENDFOREACH ()
  endif ()

  # Logging
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/radio/USRP/setup/logging.conf
          ${CMAKE_BINARY_DIR}/logging.conf
  )
  add_custom_command(
          TARGET tlkcore_lib
          PRE_BUILD # PRE_BUILD | PRE_LINK | POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_BINARY_DIR}/tlk_core_log
  )

  add_library(oai_usrpdevif MODULE usrp_lib.cpp)
  #target_include_directories(oai_usrpdevif PRIVATE Boost::boost)
  target_link_libraries(oai_usrpdevif PRIVATE ${UHD_LIBRARIES})
  target_link_libraries(oai_usrpdevif PRIVATE ${UHD_INCLUDE_DIRS})
  target_link_libraries(oai_usrpdevif PRIVATE usrp_fbs tlkcore_lib)
  target_link_libraries(oai_usrpdevif PRIVATE UTIL)
  set_target_properties(oai_usrpdevif PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  add_custom_command(TARGET oai_usrpdevif POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E create_symlink liboai_usrpdevif.so liboai_device.so
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  if (OAI_TMYTEK_UD_BBOX)
    target_compile_definitions(oai_usrpdevif PUBLIC ENABLE_TMYTEK_UD_BBOX)
  elseif (OAI_TMYTEK_AIP)
    target_compile_definitions(oai_usrpdevif PUBLIC ENABLE_TMYTEK_AIP)
  endif ()

else ()
  #find_package(Boost REQUIRED)
  find_package(UHD REQUIRED UHD)

  add_library(oai_usrpdevif MODULE usrp_lib.cpp)
  #target_include_directories(oai_usrpdevif PRIVATE Boost::boost)
  target_link_libraries(oai_usrpdevif PRIVATE ${UHD_LIBRARIES})
  target_include_directories(oai_usrpdevif PRIVATE ${UHD_INCLUDE_DIRS})
  target_link_libraries(oai_usrpdevif PRIVATE UTIL)
  set_target_properties(oai_usrpdevif PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  add_custom_command(TARGET oai_usrpdevif POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E create_symlink liboai_usrpdevif.so liboai_device.so
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()
