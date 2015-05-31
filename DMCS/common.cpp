////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    common.cpp
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周六 十一月  9 22:05:35 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

int isFDValid(int fd)
{
	return fcntl(fd, F_GETFD) != -1;
}
