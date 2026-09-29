class Medkit : MonoBehaviour
{
    int heal = 30;

	void start()
    {
        print("Medkit started on entity: " + GetOwner().GetName());

        CollisionComponent@ collider = GetOwner().GetCollider();
        if (collider != null)
        {
            SetOnTriggerEnter(collider, OnTriggerEnterCallback(this.OnTriggerEnter));
        }
    }

	void OnTriggerEnter(Entity@ other)
    {
    	//print("Medkit on enter " + other.GetName());

        ref@ pl = _GetScript(other, "Player");
        Player@ player = cast<Player>(pl);

        if (player != null)
        {
            PlaySound("assets/sounds/health_pickup.wav");

            player.health = Math::min(100, player.health + heal);

            GetOwner().active = false;
        }
    }
}