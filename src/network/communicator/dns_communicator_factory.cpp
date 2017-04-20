#include "dns_communicator_factory.h"
#include "../../http/http_request.h"
#include "../../io_service_pool.h"
#include "communicator.h"
#include "../../service_locator/service_locator.h"
#include <iostream>
namespace network {

    void dns_communicator_factory::get_connector(const http::http_request &req, connector_callback_t connector_cb, error_callback_t error_cb)
    {
        if(stopping) return error_cb(1);

        if(req.hasParameter("hostname"))
            return dns_resolver(req, std::move(connector_cb), std::move(error_cb));
        return error_cb(2); //fixme with a proper error code.
    }


    void dns_communicator_factory::dns_resolver(const http::http_request &req, connector_callback_t connector_cb, error_callback_t error_cb)
    {
        auto&& io = service::locator::service_pool().get_thread_io_service();
        std::shared_ptr<boost::asio::ip::tcp::resolver> r = std::make_shared<boost::asio::ip::tcp::resolver>(io);
        std::string port =
                (req.hasParameter("port") && req.getParameter("port").size()) ? req.getParameter("port") : ((req.ssl()) ? "443" : "80");
        /** For now we will consider only the case without ssl*/
        if(req.ssl()) return error_cb(2); //fixme
        LOGTRACE("resolving ", req.getParameter("hostname"), "with port ", port);
        boost::asio::ip::tcp::resolver::query q( req.getParameter("hostname"),  port );
        auto resolve_timer = std::make_shared<boost::asio::deadline_timer>(service::locator::service_pool().get_thread_io_service());
        resolve_timer->expires_from_now(boost::posix_time::milliseconds(resolve_timeout));
        resolve_timer->async_wait([r, resolve_timer](const boost::system::error_code &ec){
            if(!ec) r->cancel();
        });
        r->async_resolve(q,
                        [this, r, connector_cb = std::move(connector_cb), error_cb = std::move(error_cb), resolve_timer]
                        (const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator iterator)
                        {
                            resolve_timer->cancel();
                            if(ec || iterator == boost::asio::ip::tcp::resolver::iterator())
                            {
                                LOGERROR(ec.message());
                                return error_cb(3);
                            }
                            /** No error: go on connecting */

                            return endpoint_connect(std::move(iterator), std::make_shared<boost::asio::ip::tcp::socket>(service::locator::service_pool().get_thread_io_service()), std::move(connector_cb), std::move(error_cb));
                        });
    }


    void dns_communicator_factory::endpoint_connect(boost::asio::ip::tcp::resolver::iterator it, std::shared_ptr<boost::asio::ip::tcp::socket> socket, connector_callback_t connector_cb, error_callback_t error_cb)
    {
        if(it == boost::asio::ip::tcp::resolver::iterator() || stopping) //finished
            return error_cb(3);
        auto&& endpoint = *it;
        auto connect_timer = std::make_shared<boost::asio::deadline_timer>(service::locator::service_pool().get_thread_io_service());
        connect_timer->expires_from_now(boost::posix_time::milliseconds(10000));
        connect_timer->async_wait([socket, connect_timer](const boost::system::error_code &ec){
            if(!ec) socket->cancel();
        });
        ++it;
        socket->async_connect(endpoint,
                              [this, it = std::move(it), socket, connector_cb = std::move(connector_cb), error_cb = std::move(error_cb), connect_timer]
                              (const boost::system::error_code &ec)
                              {
                                  connect_timer->cancel();
                                  if(ec)
                                  {
                                      LOGERROR(ec.message());
                                      return endpoint_connect(std::move(it), std::move(socket), std::move(connector_cb), std::move(error_cb));
                                  }
                                  return connector_cb(
                                          std::unique_ptr<communicator_interface>(
                                                  new communicator<>(socket, service::locator::configuration().get_board_timeout())
                                          )
                                  );
                              });

    }


}