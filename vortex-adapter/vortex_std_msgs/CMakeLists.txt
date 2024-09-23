project(vortex_std_msgs)

find_package(ros_environment REQUIRED)

set(ROS_VERSION $ENV{ROS_VERSION})

set(msg_files
	"Vector3dWithCovariance.msg"
	"QuaterniondWithCovariance.msg"
)

# ROS 1
if(${ROS_VERSION} EQUAL 1)

	cmake_minimum_required(VERSION 2.8.3)

	# Default to C++11
	if(NOT CMAKE_CXX_STANDARD)
		set(CMAKE_CXX_STANDARD 11)
	endif()

	find_package(catkin REQUIRED
		COMPONENTS
		message_generation
		std_msgs
		geometry_msgs
	)


	add_message_files(FILES
		${msg_files}
		#DIRECTORY msg
	)

	generate_messages(DEPENDENCIES
		std_msgs
		geometry_msgs
	)

    catkin_package(
		CATKIN_DEPENDS message_runtime
	)

# ROS 2
elseif(${ROS_VERSION} EQUAL 2)

	cmake_minimum_required(VERSION 3.8)

	if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wall -Wextra -Wpedantic)
	endif()

	# find dependencies
	find_package(ament_cmake REQUIRED)
	find_package(rosidl_default_generators REQUIRED)
	find_package(std_msgs REQUIRED)
	find_package(geometry_msgs REQUIRED)

	# Apend "msg/" to each file name
	set(TEMP_LIST "")
	foreach(MSG_FILE ${msg_files})
	  list(APPEND TEMP_LIST "msg/${MSG_FILE}")
	endforeach()
	set(msg_files ${TEMP_LIST})

	rosidl_generate_interfaces(${PROJECT_NAME}
		${msg_files}
		DEPENDENCIES geometry_msgs std_msgs
	)

	ament_package()

endif()