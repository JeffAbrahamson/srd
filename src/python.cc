/*
  Copyright 2012  Jeff Abrahamson
  
  This file is part of srd.
  
  srd is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  srd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with srd.  If not, see <http://www.gnu.org/licenses/>.
*/



#include <boost/python.hpp>


#include "srd.h"


using namespace boost::python;
using namespace srd;
using namespace std;


BOOST_PYTHON_MODULE(srd)
{
    class_<root>("root", init<string, string, bool>())
	.def("add_leaf", &root::add_leaf)
	.def("get_leaf", &root::get_leaf)
	.def("set_leaf", &root::set_leaf)
	.def("rm_leaf", &root::rm_leaf)
	.def("change_password", &root::change_password);

}
