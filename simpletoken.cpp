#include "simpletoken.hpp"

#include <cmath>


void simpletoken::create( name   issuer,
                    asset  maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply    = maximum_supply;
        s.issuer        = issuer;
    });
}


void simpletoken::issue( name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( get_self() );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
        s.supply += quantity;
    });

    add_balance( get_self(), quantity, get_self() );

    if( to != get_self() ) {
        SEND_INLINE_ACTION( *this, transfer, { {get_self(), "active"_n} },
                          { get_self(), to, quantity, memo }
        );
    }
}

void simpletoken::retire( asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must retire positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void simpletoken::transfer( name    from,
                      name    to,
                      asset   quantity,
                      string  memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    name payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void simpletoken::sub_balance( name owner, asset value ) {
    accounts from_acnts( _self, owner.value );

    const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
    eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

    from_acnts.modify( from, owner, [&]( auto& a ) {
        a.balance -= value;
    });
}

void simpletoken::add_balance( name owner, asset value, name ram_payer )
{
    accounts to_acnts( _self, owner.value );
    auto to = to_acnts.find( value.symbol.code().raw() );
    if( to == to_acnts.end() ) {
        to_acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = value;
        });
    } else {
        to_acnts.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
        });
    }
}

void simpletoken::open( name owner, const symbol& symbol, name ram_payer )
{
    require_auth( ram_payer );

    auto sym_code_raw = symbol.code().raw();

    stats statstable( _self, sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
    eosio_assert( st.supply.symbol == symbol, "symbol precision mismatch" );

    accounts acnts( _self, owner.value );
    auto it = acnts.find( sym_code_raw );
    if( it == acnts.end() ) {
        acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = asset{0, symbol};
        });
    }
}

void simpletoken::close( name owner, const symbol& symbol )
{
    require_auth( owner );
    accounts acnts( _self, owner.value );
    auto it = acnts.find( symbol.code().raw() );
    eosio_assert( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
    eosio_assert( it->balance.amount == 0, "Cannot close because the balance is not zero." );
    acnts.erase( it );
}


EOSIO_DISPATCH( simpletoken,
    (create)(issue)(transfer)(open)(close)(retire)
)

