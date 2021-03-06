#pragma once

#include "../../../type.hxx"
#include "../error_code_type.hxx"
#include "../constant.hxx"

namespace usagi::json::picojson::rpc::jsonrpc20
{
  
  static inline auto get_params
  ( const object_type& request
  ) noexcept
  {
    try
    { return request.at( key_params ); }
    catch ( const std::out_of_range& e )
    { return value_type(); }
    catch ( ... )
    { return value_type(); }
  }
  
  static inline auto get_params
  ( const value_type& request
  )
  {
    try
    { return get_params( request.get< object_type >() ); }
    catch ( const std::runtime_error& e )
    { throw exception_type( error_code_type::invalid_request, e.what() ); }
    catch ( ... )
    { throw exception_type( error_code_type::invalid_request ); }
  }
  
}