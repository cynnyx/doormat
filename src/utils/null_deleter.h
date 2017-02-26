#ifndef DOORMAT_NULL_DELETER_H
#define DOORMAT_NULL_DELETER_H

/**
 * @brief The null_deleter struct is a no-op deleter.
 *
 * This has been moved around along boost releases,
 * so we hardcode it here to be sure we'll find it.
 */
struct null_deleter
{
	typedef void result_type;

	template< typename T >
	void operator() (T*) const noexcept {}
};


#endif //DOORMAT_NULL_DELETER_H
