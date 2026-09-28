class Medkit : MonoBehaviour
{
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
    	print("Medkit on enter " + other.GetName());
    }
}