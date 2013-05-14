
FUNCTION( GET_SYSTEM_NAME RETURN_SYSTEM_NAME )
	IF( WIN32 )
		SET( SYSTEM_NAME "Windows" )
	ELSEIF( APPLE )
		EXECUTE_PROCESS(COMMAND sw_vers -productName RESULT_VARIABLE COMMAND_RESULT OUTPUT_VARIABLE PRODUCT_NAME )
		STRING( STRIP ${PRODUCT_NAME} PRODUCT_NAME )
		EXECUTE_PROCESS(COMMAND sw_vers -productVersion RESULT_VARIABLE COMMAND_RESULT OUTPUT_VARIABLE PRODUCT_VERSION )
		STRING( STRIP ${PRODUCT_VERSION} PRODUCT_VERSION )
		STRING( REPLACE " " "-" SYSTEM_NAME "${PRODUCT_NAME} ${PRODUCT_VERSION}" )
	ELSEIF( UNIX )
		SET( SYSTEM_NAME "Linux" )
		EXECUTE_PROCESS(COMMAND cat /etc/issue RESULT_VARIABLE COMMAND_RESULT OUTPUT_VARIABLE COMMAND_OUTPUT ERROR_VARIABLE ERROR_OUTPUT)
		IF( COMMAND_RESULT EQUAL 0 )
			STRING( REGEX MATCH "^[^\\]*" COMMAND_OUTPUT ${COMMAND_OUTPUT} )
			STRING( STRIP ${COMMAND_OUTPUT} COMMAND_OUTPUT )
			message(STATUS "comm out: ${COMMAND_OUTPUT}" )
			STRING( REPLACE " " "-" SYSTEM_NAME ${COMMAND_OUTPUT} )
		ENDIF()
	ELSE()
		MESSAGE( FATAL_ERROR "Don't yet know this system." )
	ENDIF()

	SET( ${RETURN_SYSTEM_NAME} ${SYSTEM_NAME} PARENT_SCOPE)
ENDFUNCTION()

