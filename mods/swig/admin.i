%module(package="ctrlproxy") admin
%{
#include "../admin.h"
%}
%ignore register_admin_command;
%include "common.i";
%rename(AdminCommand) admin_command;
%immutable admin_command::help;
%immutable admin_command::help_details;
%include "../admin.h";
