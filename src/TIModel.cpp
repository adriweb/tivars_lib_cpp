
/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/TIs_lib
 * License: MIT
 */

namespace tivars;

class TIModel
{
    private $orderID = -1;
    private $name    = 'Unknown';
    private $flags   = 0;
    private $sig     = '';

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }

    /**
     * @return int
     */
    public function getFlags()
    {
        return $this->flags;
    }

    /**
     * @return string
     */
    public function getSig()
    {
        return $this->sig;
    }


    public function supportsType(TIVarType $type)
    {
        $exts = $type->getExts();
        return isset($exts[$this->orderID]) && $exts[$this->orderID] !== null;
    }

    /*** "Constructors" ***/

    /**
     * @param   int     $flags  The version compatibliity flags
     * @return  TIModel
     * @throws  \Exception
     */
    public static function createFromFlags($flags = -1)
    {
        if (TIModels::isValidFlags($flags))
        {
            $instance = new self();
            $instance->flags = $flags;
            $instance->orderID = TIModels::getDefaultOrderIDFromFlags($flags);
            $instance->sig = TIModels::getSignatureFromFlags($flags);
            $instance->name = TIModels::getDefaultNameFromFlags($flags);
            return $instance;
        } else {
            throw new \Exception("Invalid version ID");
        }
    }

    /**
     * @param   string  $name   The version name
     * @return  TIModel
     * @throws  \Exception
     */
    public static function createFromName($name = '')
    {
        if (TIModels::isValidName($name))
        {
            $instance = new self();
            $instance->name = $name;
            $instance->orderID = TIModels::getOrderIDFromName($name);
            $instance->flags = TIModels::getFlagsFromName($name);
            $instance->sig = TIModels::getSignatureFromName($name);
            return $instance;
        } else {
            throw new \Exception("Invalid version name");
        }
    }

    /**
     * @param   string  $sig    The signature (magic bytes)
     * @return  TIModel
     * @throws  \Exception
     */
    public static function createFromSignature($sig = '')
    {
        if (TIModels::isValidSignature($sig))
        {
            $instance = new self();
            $instance->sig = $sig;
            $instance->orderID = TIModels::getDefaultOrderIDFromSignature($sig);
            $instance->flags = TIModels::getMinFlagsFromSignature($sig);
            $instance->name = TIModels::getDefaultNameFromSignature($sig);
            return $instance;
        } else {
            throw new \Exception("Invalid version signature");
        }
    }

}
