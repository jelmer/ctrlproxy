%module(package="ctrlproxy") admin
%{
#include "../admin.h"
%}
%ignore register_admin_command;
%include "common.i";
%include "../admin.h";
