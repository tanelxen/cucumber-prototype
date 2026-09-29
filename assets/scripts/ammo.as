class AmmoClip : MonoBehaviour
{
    int clipSize = 64;

	void start()
    {
        CollisionComponent@ collider = GetOwner().GetCollider();
        if (collider != null)
        {
            SetOnTriggerEnter(collider, OnTriggerEnterCallback(this.OnTriggerEnter));
        }
    }

	void OnTriggerEnter(Entity@ other)
    {
        Entity@ weapon = FindEntityByName("Weapon");

        ref@ r = _GetScript(weapon, "WeaponMP40");
        WeaponMP40@ mp40 = cast<WeaponMP40>(r);

        if (mp40 != null)
        {
            PlaySound("assets/sounds/am_pkup.wav");

            mp40.ammo += clipSize;
            GetOwner().active = false;
        }
    }
}