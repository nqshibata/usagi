#include <imgui.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <map>
#include <set>
#include <stack>
#include <string>
#include <functional>
#include <random>
#include <mutex>
#include <atomic>
#include <cstddef>

namespace usagi
{
  namespace imgui
  {
    /// @brief ImGui の menu 系 API のステートフルラッパー
    /// @note Q: Is this a thread-safe? => Yes.
    struct statefull_menu_type
    {
      using functor_type = std::function< auto () -> void >;
      using value_type = std::pair< std::string, functor_type >;
      using self_type = statefull_menu_type;
      
      statefull_menu_type( const bool in_is_popup = true, const bool in_ignore_exceptions = false )
        : _root_node_label
          ( "##" + std::to_string( _generate_random_number() )
          )
        , _is_popup( in_is_popup )
        , _ignore_exceptions( in_ignore_exceptions )
      { }
      
      /// @brief open the context menu
      auto operator()()
        -> std::unique_ptr< std::string >
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        
        // このタイミングで削除処理を行わないと ImGui の描画タイミングの都合存在しないメモリー領域を参照する可能性がある
        _remove();
        _add();
        
        if ( _is_popup )
        {
          if ( ImGui::BeginPopup( _root_node_label.c_str() ) )
          {
            _recursive_menu_render( _m );
            ImGui::EndPopup();
          }
        }
        else
          _recursive_menu_render( _m );
        
        return { };
      }
      
      auto remove( const std::string& concatenated_path )
      {
        std::lock_guard< decltype( _mutex_removes ) > lock_guard( _mutex_removes );
        _removes.emplace( concatenated_path );
      }
      auto add( const std::string& concatenated_path, const std::function< auto () -> void >& f )
      {
        std::lock_guard< decltype( _mutex_adds ) > lock_guard( _mutex_adds );
        _adds.emplace( concatenated_path, f );
      }
      
      auto add( const value_type& head )
      {
        add( head.first, head.second );
      }
      
      template < typename ... T >
      auto add( const value_type& head, const T ... tails )
      {
        add( head );
        add( tails ... );
      }
      
      /// @todo 手抜き実装なので気が向いたら最適化する
      auto modify( const std::string& concatenated_path, const functor_type& f )
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        
        remove( concatenated_path );
        add( concatenated_path, f );
      }
      
      /// @note default label will be "##xxxxxx", `xxxxxx` is a random number generated by mt19937_64 and random-device's seed.
      auto root_node_label()
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        return _root_node_label;
      }
      auto root_node_label( const std::string& in )
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        return _root_node_label = in;
      }
      auto root_node_label( std::string&& in )
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        return _root_node_label = std::forward< std::string >( in );
      }
      
      auto path_separator()
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        return _path_separator;
      }
      auto path_separator( const std::string& in )
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        return _path_separator = in;
      }
      auto path_separator( std::string&& in )
      {
        std::lock_guard< decltype( _mutex ) > lock_guard( _mutex );
        return _path_separator = std::forward< std::string >( in );
      }
      
      auto is_popup() -> bool { return _is_popup; }
      auto is_popup( const bool in ) -> bool { return _is_popup = in; }
      
      auto ignore_exceptions() -> bool { return _ignore_exceptions; }
      auto ignore_exceptions( const bool in ) -> bool { return _ignore_exceptions = in; }
      
      auto show() { ImGui::OpenPopup( _root_node_label.c_str() ); }
      
      auto empty() { return _m.empty(); }
      
    private:
      
      auto _split( const std::string& in ) -> std::vector< std::string >
      {
        std::vector< std::string > r;
        boost::algorithm::split( r, in, boost::is_any_of( _path_separator ) );
        return r;
      }
      
      static auto _generate_random_number() -> std::mt19937_64::result_type
      {
        static std::random_device rnd;
        static std::mt19937_64 rne( rnd() );
        return rne();
      }
      
      using indices_type = std::vector< std::size_t >;
      struct recursive_mapper_type;
      using internal_mapper_type = std::map< std::string, recursive_mapper_type >;
      struct recursive_mapper_type
        : public internal_mapper_type
      {
        recursive_mapper_type() { }
        explicit recursive_mapper_type( const functor_type& f ) : _f( f ) { }
        explicit recursive_mapper_type( functor_type&& f ) : _f( std::forward< functor_type >( f ) ) { }
        auto operator()() const { _f(); }
        functor_type _f;
      };
      
      auto _remove()
        -> void
      {
        std::lock_guard< decltype( _mutex_removes ) > lock_guard( _mutex_removes );
        
        for ( const auto concatenated_path : _removes )
        {
          const auto path_parts = _split( concatenated_path );
          
          auto m = &_m;
          
          std::stack
          < std::pair
            < recursive_mapper_type*
            , typename recursive_mapper_type::iterator
            >
          > erase_candidates;
          
          for ( const auto p : path_parts )
          {
            const auto i = m->find( p );
            
            if ( i == cend( *m ) and not _ignore_exceptions )
              throw std::runtime_error( "usagi::imgui::statefull_menu_type::remove: cannot find the given path `" + concatenated_path + "`." );
            
            erase_candidates.emplace( std::make_pair( m, i ) );
            
            m = &i->second;
          }
          
          for ( auto* p = &erase_candidates.top(); not erase_candidates.empty(); erase_candidates.pop(), p = &erase_candidates.top() )
          {
            if ( not p->second->second.empty() )
              break;
            p->first->erase( p->second );
          }
        }
        
        _removes.clear();
      }
      
      /// @param path eg. "xxx/yyy/zzz" then xxx and yyy are intermediary node its like a directory, and zzz is marginal leaf node.
      /// @note you can change a path separator to use `path_separator( "your-separator" )`.
      auto _add()
        -> void
      {
        std::lock_guard< decltype( _mutex_adds ) > lock_guard( _mutex_adds );
        
        for ( const auto& in : _adds )
        {
          const auto& path_parts = _split( in.first );
          
          auto m = &_m;
          
          for ( const auto& p : path_parts )
          {
            const auto i = m->find( p );
            if ( i == cend( *m ) )
            {
              bool is_succeeded = false;
              typename recursive_mapper_type::iterator inserted;
              
              std::tie( inserted, is_succeeded ) = m->emplace( p, recursive_mapper_type( in.second ) );
              
              if ( not is_succeeded and not _ignore_exceptions )
                throw std::runtime_error( "usagi::imgui::statefull_menu_type::add_menu_path: failed. given path are: " + in.first );
              
              m = &inserted->second;
            }
            else
              m = &i->second;
          }
        }
        
        _adds.clear();
      }
      
      auto _recursive_menu_render( const recursive_mapper_type& m )
        -> void
      {
        if ( m.empty() )
          return;
        
        for ( const auto p : m )
        {
          if ( p.second.empty() )
          {
            if ( ImGui::MenuItem( p.first.c_str(), "", false ) )
              p.second();
          }
          else
          {
            if ( ImGui::BeginMenu( p.first.c_str() ) )
            {
              _recursive_menu_render( p.second );
              ImGui::EndMenu();
            }
          }
        }
      }
      
      /// @brief internal recursive mapper
      recursive_mapper_type _m;
      
      std::string _root_node_label;
      std::string _path_separator = "/";
      std::atomic< bool > _is_popup;
      std::atomic< bool > _ignore_exceptions;
      std::mutex _mutex;
      std::mutex _mutex_removes;
      std::mutex _mutex_adds;
      
      std::set< std::string > _removes;
      std::map< std::string, functor_type > _adds;
    };
    
  }
}
